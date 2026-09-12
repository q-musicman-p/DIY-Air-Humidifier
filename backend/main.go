package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"

	mqtt "github.com/eclipse/paho.mqtt.golang"
	"github.com/gorilla/websocket"
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
	clientReg   = make(chan WebClient)
	clientUnreg = make(chan chan Telemetry)
)

func stateDispatcher() {
	activeClients := make(map[chan Telemetry]bool)

	var latestData Telemetry

	for {
		select {
		case data := <-mqqtInbound:
			latestData = data
			for clientCh := range activeClients {
				select {
				case clientCh <- data:
				default:
				}
			}
		case client := <-clientReg:
			activeClients[client.ch] = true
			client.ch <- latestData
		case clientCh := <-clientUnreg:
			delete(activeClients, clientCh)
			close(clientCh)
		}
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
	go stateDispatcher()

	opts := mqtt.NewClientOptions()
	opts.AddBroker("tcp://localhost:1883")
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

	fmt.Println("Backend started on http://localhost:8080")
	err := http.ListenAndServe(":8080", nil)
	if err != nil {
		panic(err)
	}
}
