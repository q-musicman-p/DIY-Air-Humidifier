package main

import (
	"encoding/json"
	"errors"
	"log"
	"net/http"
	"os"
	"os/signal"

	mqtt "github.com/eclipse/paho.mqtt.golang"
	"github.com/gorilla/websocket"
	influxdb2 "github.com/influxdata/influxdb-client-go/v2"
	"github.com/influxdata/influxdb-client-go/v2/api"
)

type Telemetry struct {
	Temperature int `json:"temperature"`
	Humidity    int `json:"humidity"`
}

type WebClient struct {
	ch          chan Telemetry
	isNewClient bool
}

var (
	upgrader = websocket.Upgrader{
		CheckOrigin: func(r *http.Request) bool {
			return true
		},
	}
	mqqtInbound = make(chan Telemetry)
	influxChan  = make(chan Telemetry)
	clientReg   = make(chan WebClient)
	clientUnreg = make(chan chan Telemetry)
)

func influxWriter(writeAPI api.WriteAPI) {
	for data := range influxChan {
		writeAPI.WritePoint(
			influxdb2.NewPointWithMeasurement("telemetry").
			AddField("temperature", data.Temperature).
			AddField("humidity", data.Humidity))
	}

	writeAPI.Flush()
}

func stateDispatcher() {
	activeClients := make(map[chan Telemetry]bool)
	var latestData Telemetry

MainLoop:
	for {  
		select {
		case data, ok := <-mqqtInbound:
			if !ok {
				break MainLoop
			}

			latestData = data
			for clientCh := range activeClients {
				select {
				case clientCh <- data:
				default:
				}
			}
			select {
			case influxChan <- data:
			default:
			}
		case client := <-clientReg:
			activeClients[client.ch] = true
			client.ch <- latestData
		case clientCh := <-clientUnreg:
			delete(activeClients, clientCh)
			close(clientCh)
		}
	}

	log.Printf("Dispatcher was stopped")
	close(influxChan)
	for clientCh := range activeClients {
		close(clientCh)
	}
}

var messagePubHandler mqtt.MessageHandler = func(client mqtt.Client, msg mqtt.Message) {
	var telemetry Telemetry
	if err := json.Unmarshal(msg.Payload(), &telemetry); err != nil {
		return
	}
	mqqtInbound <- telemetry
}

func telemetryConnection(w http.ResponseWriter, r *http.Request)  {
	ws, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Println(err)
		return
	}
	defer ws.Close()

	myChan := make(chan Telemetry, 5)

	clientReg <- WebClient{ch: myChan, isNewClient: false}

	defer func() {
		clientUnreg <- myChan
	}()

	for data := range myChan {
		jsonData, err := json.Marshal(data)
		if err != nil {
			log.Println(err)
			break
		}
		if err = ws.WriteMessage(websocket.TextMessage, jsonData); err != nil {
			log.Println(err)
			break
		}
	}
}

func main() {
	mqttURL := os.Getenv("MQTT_BROKER_URL")
	influxURL := os.Getenv("INFLUXDB_URL")
	influxToken := os.Getenv("INFLUXDB_TOKEN")
	influxOrg := os.Getenv("INFLUXDB_ORG")
	influxBucket := os.Getenv("INFLUXDB_BUCKET")

	go stateDispatcher()

	influxClient := influxdb2.NewClient(influxURL, influxToken)
	defer influxClient.Close()

	writeAPI := influxClient.WriteAPI(influxOrg, influxBucket)
	errorsCh := writeAPI.Errors()
	go func() {
		for err := range errorsCh {
			log.Printf("Error while writing to InfluxDB: %v\n", err)
		}
	}()
	go influxWriter(writeAPI)

	opts := mqtt.NewClientOptions()
	opts.AddBroker(mqttURL)
	opts.SetClientID("go_backend_client")
	opts.SetDefaultPublishHandler(messagePubHandler)

	mqttClient := mqtt.NewClient(opts)
	if token := mqttClient.Connect(); token.Wait() && token.Error() != nil {
		log.Fatalf("Error while connecting to MQTT broker: %v", token.Error())
	}

	topic := "esp32/telemetry"
	if token := mqttClient.Subscribe(topic, 1, nil); token.Wait() && token.Error() != nil {
		log.Fatalf("Error while subscribing on topic: %v", token.Error())
	}
	log.Printf("Successfully subscribed on topic: %s\n", topic)

	http.HandleFunc("/telemetry", telemetryConnection)
	go func () {
		if err := http.ListenAndServe(":8080", nil); err != nil && !errors.Is(err, http.ErrServerClosed) {
			log.Fatal(err)
		}
	}()

	stopSignal := make(chan os.Signal, 1)
	signal.Notify(stopSignal, os.Interrupt)
	<-stopSignal

	mqttClient.Disconnect(250)
	close(mqqtInbound)

}
