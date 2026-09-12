import React, { useEffect, useState } from 'react';
import styles from './Telemetry.module.css'

type ConnectionStatus = 'connecting' | 'connected' | 'disconnected';

const statusClasses: Record<ConnectionStatus, string> = {
  connected: styles.statusConnected,
  connecting: styles.statusConnecting,
  disconnected: styles.statusDisconnected,
};

interface TelemetryData {
  temperature: number;
  humidity: number;
}

export const Telemetry: React.FC = () => {
  const [data, setData] = useState<TelemetryData | null>(null)
  const [connectionStatus, setConnectionStatus] = useState<ConnectionStatus>('connecting')

  useEffect(() => {
    const socket = new WebSocket('ws://localhost:8080/telemetry');

    socket.onopen = () => {
      setConnectionStatus('connected')
    };

    socket.onmessage = (event: MessageEvent) => {
      try {
        const parsedData: TelemetryData = JSON.parse(event.data);
        setData(parsedData);
      } catch (error) {
        console.error(error instanceof Error ? error.message : 'Error while JSON was parsing');
      }
    };

    socket.onclose = () => {
      setConnectionStatus('disconnected');
    };

    return () => {
      socket.close();
    };
  }, []);

  return (
    <main className={styles.main}>
      <section className={styles.card}>
        <h2 className={styles.title}>Data Telemetry</h2>

        <div className={styles.connectionStatus}>
          Status:{' '}
          <span className={statusClasses[connectionStatus]}>{connectionStatus}</span>
        </div>
        {data ? (
          <>
            <p className={styles.param}>
              Temperature: <span className={styles.value}>{data.temperature}</span>
            </p>
            <p className={styles.param}>
              Humidity: <span className={styles.value}>{data.humidity}</span>
            </p>
          </>
        ) : (
          <p className={styles.param}>Waiting for data...</p>
        )}
      </section>
    </main>
  );
};
