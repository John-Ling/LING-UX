import './App.css'
import { useCallback, useEffect } from 'react'
import { useXTerm } from 'react-xtermjs'

function App() {
  const { instance, ref } = useXTerm();

  useEffect(() => {
    instance?.writeln('Hello from react-xtermjs!')
    instance?.onData((data) => instance?.write(data))
    instance?.onKey(handle_key_down)
  }, [instance]);

  const handle_key_down = (arg1: {_: string, domEvent: KeyboardEvent}) => {
    const event = arg1.domEvent;

    if (event.key === "Enter") {
      instance?.clear();
    } else if (event.ctrlKey && event.key.toLowerCase() === 'c') {
      console.log('Ctrl+C detected!');
      event.preventDefault();
      instance?.write("Hello\n");
    }


  }

  return <div ref={ref} style={{ width: '100%', height: '100%' }} />
}

export default App
