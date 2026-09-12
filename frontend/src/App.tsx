import { Telemetry } from "./components/Telemetry.tsx";
import './App.css';

function App() {
  return (
    <>
      <header>
        <h1>DIY Air Humidifier Project</h1>
      </header>

      <main>
        <div className='center'>
          <Telemetry />
        </div>
      </main>

      <footer>
        <p>&copy; 2026 p-musicman-q. Licensed under MIT.</p>
      </footer>
    </>
  )
}

export default App
