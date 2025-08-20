/*
* ButtonManager.ino
* 
* Button input handling with debouncing
* 
* Button Functions:
* - UP: Start/Stop recording
* - DOWN: Play/Pause current recording
* - LEFT: Previous recording
* - RIGHT: Next recording
*/

#include "SongbirdRecorder.h"

// Button state tracking for debouncing
typedef struct ButtonState
{
  bool lastReading;         // Last physical reading
  bool lastState;           // Last debounced state
  unsigned long lastTime;   // Last time state changed
  bool pressed;             // True if button was just pressed
} ButtonState;

static ButtonState upButton = {HIGH, HIGH, 0, false};
static ButtonState downButton = {HIGH, HIGH, 0, false};
static ButtonState leftButton = {HIGH, HIGH, 0, false};
static ButtonState rightButton = {HIGH, HIGH, 0, false};

bool debounceButton(struct ButtonState* btn, int pin)
{
  bool currentReading = digitalRead(pin);
  bool buttonPressed = false;

  // If the reading chnaged, reset the debounce timer
  if (currentReading != btn->lastReading) 
  {
    btn->lastTime = millis();
  }

  // If enough time has passed, check if state is actually changed
  if ((millis() - btn->lastTime) > BUTTON_DEBOUNCE_MS) 
  {
    // If the button state has changed
    if (currentReading != btn->lastState)
    {
      btn->lastState = currentReading;

      // Button was pressed (went from HIGH to LOW with pull-up)
      if (btn->lastState == LOW) 
      {
        buttonPressed = true;
      }
    }
  }

  btn->lastReading = currentReading;
  return buttonPressed;
}

void handleButtons()
{
  if (debounceButton(&upButton, BTN_UP_PIN)) 
  {
    handleUpButton();
  }

  if (debounceButton(&downButton, BTN_DOWN_PIN))
  {
    handleDownButton();
  }

  if (debounceButton(&leftButton, BTN_LEFT_PIN))
  {
    handleLeftButton();
  }

  if (debounceButton(&rightButton, BTN_RIGHT_PIN))
  {
    handleRightButton();
  }
}

void handleUpButton()
{
  Serial.println("UP button pressed");

  // Don't allow recording without SD card
  if (!sdCardReady)
  {
    Serial.println("Cannot record - no SD card");
    return;
  }

  switch (currentState)
  {
    case STATE_IDLE:
      // Start recording
      startRecording();
      break;

    case STATE_RECORDING:
      // Stop recording
      stopRecording();
      break;

    case STATE_PLAYBACK:
      // Stop playback
      stopPlayback();
      break;
  }
}

void handleDownButton()
{
  Serial.println("DOWN button pressed");

  switch (currentState) 
  {
    case STATE_IDLE:
      // Start playback if files available
      if (totalFiles > 0)
      {
        startPlayback();
      }
      else 
      {
        Serial.println("No files to play");
      }
      break;

    case STATE_RECORDING:
      // Cannot change to playback while recording
      Serial.println("Cannot play while recording");
      break;

    case STATE_PLAYBACK:
      // Stop playback
      stopPlayback();
      break;
  }
}

void handleLeftButton()
{
  Serial.println("Left button pressed");

  // Only navigate files when idle
  if (currentState != STATE_IDLE) 
  {
    Serial.println("Cannot navigate while recording/playing");
    return;
  }

  // Navigate to previous file
  if (totalFiles > 0)
  {
    currentFileIndex--;
    if (currentFileIndex < 0)
    {
      currentFileIndex = totalFiles - 1; // Wrap to the last file
    }

    loadCurrentFilename();
    Serial.print("Selected file: ");
    Serial.println(currentFilename);
  }
  else
  {
    Serial.println("No files available");
  }
}

void handleRightButton()
{
  Serial.println("RIGHT button pressed");

  // Only navigate files when idle
  if (currentState != STATE_IDLE) 
  {
    Serial.println("Cannot navigate while recording/playing");
    return;
  }

  // Navigate to next file
  if (totalFiles > 0)
  {
    currentFileIndex++;
    if (currentFileIndex >= totalFiles)
    {
      currentFileIndex = 0; // Wrap to the first file
    }

    loadCurrentFilename();

    Serial.print("Selected file: ");
    Serial.println(currentFilename);
  }
  else 
  {
    Serial.println("No files available");
  }
}