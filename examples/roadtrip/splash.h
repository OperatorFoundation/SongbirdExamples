/**
 * Splash Screen Animation for Roadtrip
 * 
 * Cinematic intro sequence:
 * 1. "Operator" with rotary phone dial
 * 2. "Presents"
 * 3. "Songbird"
 * 4. "roadtrip"
 * 5. Outrun-style road scene
 * 6. Fade to main UI
 */

#ifndef SPLASH_H
#define SPLASH_H

#include <Adafruit_SSD1306.h>

// Timing constants (in milliseconds)
#define SPLASH_FADE_DURATION    400
#define SPLASH_OPERATOR_HOLD   1500
#define SPLASH_PRESENTS_HOLD   1000
#define SPLASH_SONGBIRD_HOLD   1200
#define SPLASH_ROADTRIP_HOLD   1200
#define SPLASH_OUTRUN_HOLD     2500

// Draw a rotary phone dial (16x16 pixels)
void drawRotaryDial(Adafruit_SSD1306 &disp, int x, int y) {
  // Outer circle
  disp.drawCircle(x + 7, y + 7, 7, SSD1306_WHITE);
  // Inner circle (finger stop)
  disp.drawCircle(x + 7, y + 7, 2, SSD1306_WHITE);
  // Finger holes (10 positions around dial)
  disp.drawPixel(x + 7, y + 1, SSD1306_WHITE);   // 12 o'clock
  disp.drawPixel(x + 10, y + 2, SSD1306_WHITE);  // 1
  disp.drawPixel(x + 12, y + 4, SSD1306_WHITE);  // 2
  disp.drawPixel(x + 13, y + 7, SSD1306_WHITE);  // 3
  disp.drawPixel(x + 12, y + 10, SSD1306_WHITE); // 4
  disp.drawPixel(x + 10, y + 12, SSD1306_WHITE); // 5
  disp.drawPixel(x + 7, y + 13, SSD1306_WHITE);  // 6
  disp.drawPixel(x + 4, y + 12, SSD1306_WHITE);  // 7
  disp.drawPixel(x + 2, y + 10, SSD1306_WHITE);  // 8
  disp.drawPixel(x + 1, y + 7, SSD1306_WHITE);   // 9
}

// Draw Outrun-style road scene
void drawOutrunScene(Adafruit_SSD1306 &disp, int frame) {
  // Sky (top portion - leave blank for "sky")
  
  // Sun/horizon glow
  disp.drawPixel(64, 6, SSD1306_WHITE);
  disp.drawPixel(63, 7, SSD1306_WHITE);
  disp.drawPixel(64, 7, SSD1306_WHITE);
  disp.drawPixel(65, 7, SSD1306_WHITE);
  disp.drawPixel(62, 8, SSD1306_WHITE);
  disp.drawPixel(63, 8, SSD1306_WHITE);
  disp.drawPixel(64, 8, SSD1306_WHITE);
  disp.drawPixel(65, 8, SSD1306_WHITE);
  disp.drawPixel(66, 8, SSD1306_WHITE);
  
  // Horizon line
  disp.drawFastHLine(0, 12, 128, SSD1306_WHITE);
  
  // Mountains/hills silhouette
  for (int i = 0; i < 128; i++) {
    int h = 0;
    // Create rolling hills
    h = (int)(2 * sin(i * 0.05) + 1.5 * sin(i * 0.12 + 1) + sin(i * 0.08 + 2));
    if (h > 0) {
      disp.drawFastVLine(i, 12 - h, h, SSD1306_WHITE);
    }
  }
  
  // Road - perspective trapezoid
  // Road gets wider toward bottom
  int roadTopLeft = 58;
  int roadTopRight = 70;
  int roadBottomLeft = 20;
  int roadBottomRight = 108;
  
  // Draw road edges
  for (int y = 13; y < 32; y++) {
    float t = (y - 13) / 19.0;
    int leftX = roadTopLeft + (int)((roadBottomLeft - roadTopLeft) * t);
    int rightX = roadTopRight + (int)((roadBottomRight - roadTopRight) * t);
    disp.drawPixel(leftX, y, SSD1306_WHITE);
    disp.drawPixel(rightX, y, SSD1306_WHITE);
  }
  
  // Center dashed line (animated)
  int dashOffset = (frame / 3) % 6;
  for (int y = 14; y < 32; y++) {
    float t = (y - 13) / 19.0;
    int centerX = 64;
    // Dash pattern - gets longer toward bottom for perspective
    int dashLen = 1 + (int)(t * 2);
    int gapLen = 2 + (int)(t * 3);
    int cycle = dashLen + gapLen;
    int pos = (y + dashOffset) % cycle;
    if (pos < dashLen) {
      disp.drawPixel(centerX, y, SSD1306_WHITE);
    }
  }
  
  // Palm trees on sides (simple silhouettes)
  // Left palm
  disp.drawFastVLine(15, 8, 4, SSD1306_WHITE);
  disp.drawPixel(13, 6, SSD1306_WHITE);
  disp.drawPixel(14, 7, SSD1306_WHITE);
  disp.drawPixel(16, 7, SSD1306_WHITE);
  disp.drawPixel(17, 6, SSD1306_WHITE);
  disp.drawPixel(12, 7, SSD1306_WHITE);
  disp.drawPixel(18, 7, SSD1306_WHITE);
  
  // Right palm
  disp.drawFastVLine(112, 8, 4, SSD1306_WHITE);
  disp.drawPixel(110, 6, SSD1306_WHITE);
  disp.drawPixel(111, 7, SSD1306_WHITE);
  disp.drawPixel(113, 7, SSD1306_WHITE);
  disp.drawPixel(114, 6, SSD1306_WHITE);
  disp.drawPixel(109, 7, SSD1306_WHITE);
  disp.drawPixel(115, 7, SSD1306_WHITE);
  
  // Car hood at bottom (dashboard view hint)
  disp.drawFastHLine(44, 31, 40, SSD1306_WHITE);
  disp.drawFastHLine(48, 30, 32, SSD1306_WHITE);
  disp.drawPixel(44, 30, SSD1306_WHITE);
  disp.drawPixel(83, 30, SSD1306_WHITE);
}

// Fade effect using dithering pattern
void applyFade(Adafruit_SSD1306 &disp, int fadeLevel) {
  // fadeLevel: 0 = fully visible, 8 = fully black
  if (fadeLevel <= 0) return;
  if (fadeLevel >= 8) {
    disp.clearDisplay();
    return;
  }
  
  // Dither patterns for fade levels
  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 128; x++) {
      // Use ordered dithering based on fade level
      int threshold = ((x % 4) + (y % 4) * 4) % 8;
      if (threshold < fadeLevel) {
        disp.drawPixel(x, y, SSD1306_BLACK);
      }
    }
  }
}

// Main splash screen sequence
void playSplashScreen(Adafruit_SSD1306 &disp) {
  unsigned long startTime;
  int frame = 0;
  
  // === Scene 1: "Operator" with rotary dial ===
  // Fade in
  for (int fade = 8; fade >= 0; fade--) {
    disp.clearDisplay();
    disp.setTextSize(2);
    disp.setCursor(8, 9);
    disp.print("Operator");
    drawRotaryDial(disp, 108, 8);
    applyFade(disp, fade);
    disp.display();
    delay(SPLASH_FADE_DURATION / 8);
  }
  delay(SPLASH_OPERATOR_HOLD);
  
  // Fade out
  for (int fade = 0; fade <= 8; fade++) {
    disp.clearDisplay();
    disp.setTextSize(2);
    disp.setCursor(8, 9);
    disp.print("Operator");
    drawRotaryDial(disp, 108, 8);
    applyFade(disp, fade);
    disp.display();
    delay(SPLASH_FADE_DURATION / 8);
  }
  
  // === Scene 2: "Presents" ===
  for (int fade = 8; fade >= 0; fade--) {
    disp.clearDisplay();
    disp.setTextSize(1);
    disp.setCursor(44, 12);
    disp.print("Presents");
    applyFade(disp, fade);
    disp.display();
    delay(SPLASH_FADE_DURATION / 8);
  }
  delay(SPLASH_PRESENTS_HOLD);
  
  for (int fade = 0; fade <= 8; fade++) {
    disp.clearDisplay();
    disp.setTextSize(1);
    disp.setCursor(44, 12);
    disp.print("Presents");
    applyFade(disp, fade);
    disp.display();
    delay(SPLASH_FADE_DURATION / 8);
  }
  
  // === Scene 3: "Songbird" ===
  for (int fade = 8; fade >= 0; fade--) {
    disp.clearDisplay();
    disp.setTextSize(2);
    disp.setCursor(20, 9);
    disp.print("Songbird");
    applyFade(disp, fade);
    disp.display();
    delay(SPLASH_FADE_DURATION / 8);
  }
  delay(SPLASH_SONGBIRD_HOLD);
  
  for (int fade = 0; fade <= 8; fade++) {
    disp.clearDisplay();
    disp.setTextSize(2);
    disp.setCursor(20, 9);
    disp.print("Songbird");
    applyFade(disp, fade);
    disp.display();
    delay(SPLASH_FADE_DURATION / 8);
  }
  
  // === Scene 4: "roadtrip" ===
  for (int fade = 8; fade >= 0; fade--) {
    disp.clearDisplay();
    disp.setTextSize(2);
    disp.setCursor(16, 9);
    disp.print("roadtrip");
    applyFade(disp, fade);
    disp.display();
    delay(SPLASH_FADE_DURATION / 8);
  }
  delay(SPLASH_ROADTRIP_HOLD);
  
  for (int fade = 0; fade <= 8; fade++) {
    disp.clearDisplay();
    disp.setTextSize(2);
    disp.setCursor(16, 9);
    disp.print("roadtrip");
    applyFade(disp, fade);
    disp.display();
    delay(SPLASH_FADE_DURATION / 8);
  }
  
  // === Scene 5: Outrun road scene ===
  startTime = millis();
  // Fade in
  for (int fade = 8; fade >= 0; fade--) {
    disp.clearDisplay();
    drawOutrunScene(disp, frame++);
    applyFade(disp, fade);
    disp.display();
    delay(SPLASH_FADE_DURATION / 8);
  }
  
  // Animate for a while
  startTime = millis();
  while (millis() - startTime < SPLASH_OUTRUN_HOLD) {
    disp.clearDisplay();
    drawOutrunScene(disp, frame++);
    disp.display();
    delay(50);
  }
  
  // Fade out
  for (int fade = 0; fade <= 8; fade++) {
    disp.clearDisplay();
    drawOutrunScene(disp, frame++);
    applyFade(disp, fade);
    disp.display();
    delay(SPLASH_FADE_DURATION / 8);
  }
  
  // Brief pause before main UI
  delay(200);
}

#endif // SPLASH_H