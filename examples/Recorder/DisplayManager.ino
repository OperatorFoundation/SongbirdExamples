/*
 * DisplayManager.ino
 * 
 * OLED display management and user interface
 * 
 * Displays:
 * - Current state (idle, recording, playback)
 * - Recording timer and audio level
 * - Current filename and file navigation
 * - Button controls and status
 */

#include "SongbirdRecorder.h"

void initializeDisplay()
{
  Serial.println("Initializing display...");

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
  {
    Serial.println("Display initialization failed");
    return;
  }

  display.setRotation(2);  // Rotate display 180 degrees

  resetDisplay();

  // Show startup message
  display.println("Songbird Recorder");
  display.println("Initializing...");
  display.display();

  Serial.println("Display initialized");
}

void resetDisplay()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
}

void updateDisplay()
{
  resetDisplay();

  // Line 1: Status and timing
  if (currentState == STATE_RECORDING)
  {
    // Show recording status with timer
    showRecordingStatus();
  }
  else if (currentState == STATE_PLAYBACK)
  {
    showPlaybackStatus();
  }
  else
  {
    showIdleStatus();
  }

  // DEBUG:
  // display.setCursor(0, 24);  // Bottom line
  // display.print("USB Lvl: ");
  // float level = getInputLevel();
  // display.print(level, 2);  // Show level with 2 decimal places

  // if (level > 0.01) 
  // {
  //   display.print(" AUDIO");
  // } 
  // else 
  // {
  //   display.print(" QUIET");
  // }

  display.display();
}

void showRecordingStatus()
{
  // Line 1: Status and timing

  // Show recording status with timer
  unsigned long recordTime = (millis() - recordStartTime) / 1000;
  display.print("REC ");
  display.print(recordTime / 60);
  display.print(":");
  if (recordTime % 60 < 10) display.print("0");
  display.print(recordTime % 60);
  
  // Show audio level
  float level = getInputLevel();
  display.print("  L:");
  drawProgressBar(80, 2, 40, 6, level);

  // Line 2: Current filename or status
  showFileStatus();

  // Line 3: Button controls
  display.setCursor(0, 24);
  display.print("UP:Stop recording");
}

void showPlaybackStatus()
{
  // Line 1: Status and timing

  // Show playback status
  display.print("PLAY");
  if (playWav.isPlaying()) 
  {
    unsigned long playTime = playWav.positionMillis() / 1000;
    display.print(" ");
    display.print(playTime / 60);
    display.print(":");
    if (playTime % 60 < 10) display.print("0");
    display.print(playTime % 60);
  }

  // Line 2: Current filename or status
  showFileStatus();

  // Line 3: Button controls
  display.setCursor(0, 24);
  display.print("UP/DN:Stop playback");
}

void showIdleStatus()
{
  if (!sdCardReady)
  {
    // Show prominent SD card message
    display.print("INSERT SD CARD");
    
    // Line 2: instruction
    display.setCursor(0, 12);
    display.print("Please insert SD card");
    
    // Line 3: Status
    display.setCursor(0, 24);
    display.print("Waiting for SD card...");
  }
  else 
  {
    // Line 1: Status and timing
    // Show idle status
    display.print("IDLE");
    if (totalFiles > 0) 
    {
      display.print("  [");
      display.print(currentFileIndex + 1);
      display.print("/");
      display.print(totalFiles);
      display.print("]");
    }

    // Line 2: Current filename or status
    showFileStatus();

    // Line 3: Button controls
    display.setCursor(0, 24);

    if (totalFiles > 0) 
    {
      display.print("UP:Rec DN:Play L/R:Nav");
    } 
    else 
    {
      display.print("UP:Record");
    }
  }
}

void showFileStatus()
{
  display.setCursor(0, 12);
  if (currentFilename.length() > 0)
  {
    String displayName = currentFilename;

    // Truncate filename if too long
    if (displayName.length() > 21) 
    {
      displayName = displayName.substring(0, 18) + "...";
    }
    display.print(displayName);
  }
  else if (totalFiles == 0)
  {
    display.print("No recordings");
  }
  else if (!sdCardReady) 
  {
    display.print("SD card not ready");
  }
}

void drawProgressBar(int x, int y, int width, int height, float level) 
{
  // Draw progress bar border
  display.drawRect(x, y, width, height, SSD1306_WHITE);
  
  // Fill progress bar based on level (0.0 to 1.0)
  int fillWidth = (int)(level * (width - 2));
  if (fillWidth > 0) 
  {
    display.fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
  }
}
