import { useRef, useCallback } from "react";
import idle from "../assets/wav/idle_3.wav";

import beep from "../assets/wav/beep.wav";
import spinup from "../assets/wav/spinup.wav";
import hdd_click from "../assets/wav/hdd_click.wav";
import startup_click from "../assets/wav/startup_click.wav";

type SoundKey = keyof typeof sounds;

const sounds = {
  idle,
  beep,
  spinup,
  hdd_click,
  startup_click,
};

export const useSound = (soundKey: SoundKey) => {
  const audioRef = useRef<HTMLAudioElement | null>(null);
  const volumes = {
    idle: 0.4,
    beep: 0.4,
    spinup: 0.4,
    hdd_click: 0.8,
    startup_click: 0.4,
  };

  const getAudio = useCallback(() => {
    if (!audioRef.current) {
      audioRef.current = new Audio(sounds[soundKey]);
    }
    return audioRef.current;
  }, [soundKey]);

  const playOnce = useCallback(() => {
    const audio = getAudio();
    audio.loop = false;
    audio.currentTime = 0;
    audio.volume = volumes[soundKey] || 1.0;
    audio.play();
  }, [getAudio]);

  const playLoop = useCallback(() => {
    const audio = getAudio();
    audio.loop = true;
    audio.currentTime = 0;
    audio.volume = volumes[soundKey] || 1.0;
    audio.play();
  }, [getAudio]);

  const stop = useCallback(() => {
    if (audioRef.current) {
      audioRef.current.pause();
      audioRef.current.currentTime = 0;
    }
  }, []);

  return { playOnce, playLoop, stop };
};