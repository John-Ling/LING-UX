import { useRef, useCallback } from "react";
import idle from "../assets/wav/idle_2.wav";
import beep from "../assets/wav/beep.wav";
import startup_click from "../assets/wav/startup_click.wav";

type SoundKey = keyof typeof sounds;

const sounds = {
  idle,
  beep,
  startup_click,
};

export const useSound = (soundKey: SoundKey) => {
  const audioRef = useRef<HTMLAudioElement | null>(null);

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
    audio.play();
  }, [getAudio]);

  const playLoop = useCallback(() => {
    const audio = getAudio();
    audio.loop = true;
    audio.currentTime = 0;
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