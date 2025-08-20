/*
* AudioManager.ino
* 
* Audio processing and routing for phone call recording
* 
* Audio Flow:
* - Phone audio (USB) → headset output (for user to hear)
* - Headset mic → phone output (user's voice to phone)
* - Both streams → mixer → recording queue → SD card
* - Playback → headset output (to hear recordings)
*/

#include "SongbirdRecorder.h"

// Audio connections - Core phone call routing
AudioConnection patchCord1(inputFromPhone, 0, leftHeadphonesMixer, 0);   // Phone left to left mixer
AudioConnection patchCord2(inputFromPhone, 1, rightHeadphonesMixer, 0);  // Phone right to right mixer
AudioConnection patchCord3(leftHeadphonesMixer, 0, outputToHeadset, 0);  // Left mixer to left output
AudioConnection patchCord4(rightHeadphonesMixer, 0, outputToHeadset, 1); // Right mixer to right output

// Microphone passthrough
AudioConnection patchCord5(inputFromHeadset, 0, phoneOutputMixer, 0);  // Headset mic to mixer
AudioConnection patchCord5b(recordBeep, 0, phoneOutputMixer, 1);       // Beep to mixer
AudioConnection patchCord5c(phoneOutputMixer, 0, outputToPhone, 0);    // Mixed output to phone

// Recording connections
AudioConnection patchCord6(inputFromPhone, 0, phoneMixer, 0);           // Phone left for recording
AudioConnection patchCord7(inputFromPhone, 1, phoneMixer, 1);           // Phone right for recording  
AudioConnection patchCord8(inputFromHeadset, 0, phoneMixer, 2);         // Headset mic for recording
AudioConnection patchCord9(phoneMixer, 0, recordQueue, 0);              // Mixed audio to recorder
AudioConnection patchCord10(phoneMixer, 0, inputLevel, 0);              // Audio level monitoring
AudioConnection patchCordBeep(recordBeep, 0, leftHeadphonesMixer, 2);   // Beep to left headphone mixer
AudioConnection patchCordBeep2(recordBeep, 0, rightHeadphonesMixer, 2); // Beep to right headphone mixer

// Playback connections
AudioConnection patchCord11(playWav, 0, leftHeadphonesMixer, 1);   // Playback left to left mixer
AudioConnection patchCord12(playWav, 1, rightHeadphonesMixer, 1);  // Playback right to right mixer

void setupAudioProcessing()
{
  Serial.println("Initializing audio processing...");

  // Allocate memory
  AudioMemory(AUDIO_MEMORY_BLOCKS);
  
  // Setup headphone amplifier
  pinMode(HPAMP_VOL_CLK, OUTPUT);
  pinMode(HPAMP_VOL_UD, OUTPUT);
  pinMode(HPAMP_SHUTDOWN, OUTPUT);

  digitalWrite(HPAMP_VOL_CLK, LOW);
  digitalWrite(HPAMP_VOL_UD, LOW);
  digitalWrite(HPAMP_SHUTDOWN, LOW);  // LOW to enable headphone amp

  audioShield.enable();
  audioShield.volume(0.5);
  audioShield.inputSelect(AUDIO_INPUT_MIC);
  audioShield.adcHighPassFilterEnable();
  
  delay(1000);
  
  // Configure mixers
  leftHeadphonesMixer.gain(0, 0.5);   // USB phone left channel
  leftHeadphonesMixer.gain(1, 0.3);   // Playback left channel
  leftHeadphonesMixer.gain(2, 0.2);   // Beep channel
  leftHeadphonesMixer.gain(3, 0);     // Empty
  
  rightHeadphonesMixer.gain(0, 0.5);  // USB phone right channel  
  rightHeadphonesMixer.gain(1, 0.3);  // Playback right channel
  rightHeadphonesMixer.gain(2, 0.2);  // Beep channel
  rightHeadphonesMixer.gain(3, 0);    // Empty
  
  // Recording mixer
  phoneMixer.gain(0, 0.5);  // Phone left
  phoneMixer.gain(1, 0.5);  // Phone right
  phoneMixer.gain(2, 0.5);  // Headset mic
  phoneMixer.gain(3, 0.0);  // Unused

  // Phone output mixer
  phoneOutputMixer.gain(0, 1.0);  // Headset microphone
  phoneOutputMixer.gain(1, 0.3);  // Beep (lower volume)
  phoneOutputMixer.gain(2, 0.0);  // Unused
  phoneOutputMixer.gain(3, 0.0);  // Unused

  // Initialize beep generator
  recordBeep.frequency(BEEP_FREQUENCY);
  recordBeep.amplitude(0);  // Start silent

  Serial.println("Audio processing initialized");
}

void startRecording()
{
  if (!sdCardReady) 
  {
    Serial.println("Cannot record: The SD card is not ready");
    return;
  }

  if (currentState != STATE_IDLE) 
  {
    Serial.println("Cannot record: Already recording or playing");
    return;
  }

  // Play beep to indicate recording start
  playRecordingBeep();

  // Generate filename with current timestamp
  String filename = generateRecordingFilename();

  // Create WAV file
  createWAVFile(filename);

  // Start recording queue
  recordQueue.begin();
  currentState = STATE_RECORDING;
  recordStartTime = millis();

  Serial.print("Recording started: ");
  Serial.println(filename);
}

void stopRecording()
{
  if (currentState != STATE_RECORDING) 
  {
    return;
  }

  // Stop the recording queue
  recordQueue.end();

  // Finalize the WAV file
  finalizeWAVFile();

  // Update state
  currentState = STATE_IDLE;

  // Play beep to indicate recording stop
  playRecordingBeep();

  // Rescan for files
  scanForFiles();

  Serial.println("Recording stopped");
}

void handleRecording()
{
  // Check if we have reached max recording time
  if (millis() - recordStartTime > MAX_RECORDING_TIME_MS) 
  {
    Serial.println("Maximum recording time reached");
    stopRecording();
    return;
  }

  // Process available audio data

  // recordQueue.available() returns how many audio blocks are ready to be read
  // We want to process 2 blocks at a time
  while (recordQueue.available() >= 2) 
  {
    // Get audio data from the queue

    // Temporary buffer that can hold 2 audio blocks (256 + 256 = 512 bytes)
    byte buffer[RECORDING_BUFFER_SIZE];

    // Copy first buffer
    memcpy(buffer, recordQueue.readBuffer(), AUDIO_BLOCK_SIZE);
    recordQueue.freeBuffer();

    // Copy second audio block
    memcpy(buffer + AUDIO_BLOCK_SIZE, recordQueue.readBuffer(), AUDIO_BLOCK_SIZE);
    recordQueue.freeBuffer();

    // Write to WAV file
    writeWAVData(buffer, RECORDING_BUFFER_SIZE);
  }
}

void startPlayback()
{
  if (!sdCardReady || totalFiles == 0) 
  {
    Serial.println("Cannot play: No files available");
    return;
  }
  
  if (currentState != STATE_IDLE) 
  {
    Serial.println("Cannot play: Already recording or playing");
    return;
  }

  // Create the filepath
  String filepath = String(CALLS_DIRECTORY) + "/" + currentFilename;
  
  Serial.print("Attempting to play: ");
  Serial.println(filepath);

  // Try stopping any existing playback first
  playWav.stop();
  
  // Small delay to ensure cleanup
  delay(10);
  
  // Start playback
  if (playWav.play(filepath.c_str())) 
  {
    currentState = STATE_PLAYBACK;
    Serial.print("Playing: ");
    Serial.println(currentFilename);
  }
  else 
  {
    Serial.print("Failed to play: ");
    Serial.println(currentFilename);
  }
}

void playRecordingBeep() 
{
  recordBeep.amplitude(BEEP_AMPLITUDE);
  delay(BEEP_DURATION_MS);
  recordBeep.amplitude(0);
}

void stopPlayback() 
{
  if (currentState != STATE_PLAYBACK) 
  {
    return;
  }
  
  // Stop playback
  playWav.stop();
  currentState = STATE_IDLE;
  
  Serial.println("Playback stopped");
}

float getInputLevel()
{
  // Return RMS level of input audio (0.0 to 1.0)
  if (inputLevel.available()) 
  {
    return inputLevel.read();
  }

  return 0.0;
}

String generateRecordingFilename()
{
  // Generate filename with timestamp
  // Format: CALL_TIMESTAMP.WAV

  String filename = "CALL_";
  filename += String(millis());
  filename += ".WAV";
  return filename;
}





