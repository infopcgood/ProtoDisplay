/* main.cpp - ProtoDisplay
 * written by Jaemin Shim (infopcgood@protonmail.com), 2024. 
 * Please contact me via email or Discord!
 */

// Include libraries
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <PNGdec.h>
#include <MatrixHardware_Teensy4_ShieldV5.h>
#include <SmartMatrix.h>
#include <SPI.h>

// Set variables for SmartMatrix library (copied from GitHub page)
#define COLOR_DEPTH 24                  // Choose the color depth used for storing pixels in the layers: 24 or 48 (24 is good for most sketches - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24)
const uint16_t kMatrixWidth = 64;       // Set to the width of your display, must be a multiple of 8
const uint16_t kMatrixHeight = 64;      // Set to the height of your display
const uint8_t kRefreshDepth = 36;       // Tradeoff of color quality vs refresh rate, max brightness, and RAM usage.  36 is typically good, drop down to 24 if you need to.  On Teensy, multiples of 3, up to 48: 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48.  On ESP32: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save RAM, more to keep from dropping frames and automatically lowering refresh rate.  (This isn't used on ESP32, leave as default)
const uint8_t kPanelType = SM_PANELTYPE_HUB75_32ROW_MOD16SCAN;   // Choose the configuration that matches your panels.  See more details in MatrixCommonHub75.h and the docs: https://github.com/pixelmatix/SmartMatrix/wiki
const uint32_t kMatrixOptions = (SM_HUB75_OPTIONS_NONE);         // see docs for options: https://github.com/pixelmatix/SmartMatrix/wiki
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);

// Allocate buffers for matrix LED display (also copied from GitHub page)
SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);

// Constants for facial express control
rgb24 faceColor = {24, 152, 220};
const unsigned short dataLength = 7;
const unsigned short mouthWidthDataIndex = 1;
const unsigned short mouthHeightDataIndex = 2;
const unsigned short leftEyeDataIndex = 3;
const unsigned short rightEyeDataIndex = 4;
const unsigned short noseDataIndex = 5;

// Global variables for facial expression control
// Could have used char but using short, I'm not broke on memory right now
unsigned short mouthWidth = 0;          // (current mouth width) / (max mouth width) * 128, maximum 127 minimum 1 guaranteed
unsigned short mouthHeight = 0;         // (current mouth height) / (max mouth height) * 128, maximum 127 minimum 1 guaranteed
unsigned short leftEyeEAR128 = 0;       // (left eye EAR) * 128, maximum 127 minimum 1 guaranteed
unsigned short rightEyeEAR128 = 0;      // (right eye EAR) * 128, maximum 127 minimum 1 guaranteed
signed short noseOffset = 0;            // (nose offset) / |(maximum nose offset)| * 64, maximum 63 minimum -63 guaranteed
                                        // nose offset is parsed by mapping 0000001 ~ 1111111 to -63 ~ 63 unlike traditional signed variables

// Global variables for video display
char* video_filename_prefix;
char* video_filename_extension = ".png";
uint8_t frame_cnt = 134; // obviously should not be 0
uint8_t frame_idx_digits = 0; // automatically derived from frame_cnt, leave as 0
uint8_t frame_start = 1;
uint8_t video_idx = 0;
uint8_t fps = 8;

// Constants for mode transition
const unsigned short mouth_mode = 0;
const unsigned short video_mode = 1;

// Global variables for mode transition
unsigned short mode = 1;

// Other global variables
bool initialized = false;

// Other constants
const uint32_t millisInOneLoop = 50;

// Functions
void drawTextOnRGB24BackgroundLayer(SMLayerBackground<rgb24, 0U> layer, unsigned short fontWidth, int16_t x, int16_t y, rgb24 &color, const char* text) {
    const unsigned short len = strlen(text);
    for(int i = 0; i < len; i++) {
        layer.drawChar(x + fontWidth * i, y, color, text[i]);
    }
    return;
}

int readVideoConfigFromJSON(const char* filename) {
    if(!SD.exists(filename)) {
        Serial.print("File ");
        Serial.print(filename);
        Serial.println(" does not exist on SD.");
        return -1;
    }
    File videoDataFile = SD.open(filename);
    StaticJsonDocument<512> videoDataJson;
    deserializeJson(videoDataJson, videoDataFile);
    if(initialized) {
        free(video_filename_prefix);
    }
    video_filename_prefix = strdup(videoDataJson["video_filename_prefix"]);
    frame_cnt = atoi(videoDataJson["frame_cnt"]);
    video_idx = 0;
    uint8_t tmp_frame_cnt = frame_cnt;
    frame_idx_digits = 0;
    while(tmp_frame_cnt) {
        frame_idx_digits++;
        tmp_frame_cnt /= 10;
    }
    videoDataFile.close();
    return 0;
}

// Functions for PNG file processing, mostly copied from example code
File sdPNGFile;
PNG png;

void* openPNG(const char* szFilename, int32_t *pFileSize) {
    sdPNGFile = SD.open(szFilename);
    *pFileSize = sdPNGFile.size();
    return &sdPNGFile;
}

void closePNG(void* handle) {
    if(sdPNGFile) {
        sdPNGFile.close();
    }
    return;
}

int32_t readPNG(PNGFILE *handle, uint8_t *buffer, int32_t length) {
    if(sdPNGFile) {
        return sdPNGFile.read(buffer, length);
    }
    else {
        return 0;
    }
}

int32_t seekPNG(PNGFILE *handle, int32_t position) {
    if(sdPNGFile) {
        return sdPNGFile.seek(position);
    }
    else {
        return 0;
    }
}

void drawPNG(PNGDRAW *pDraw) {
    uint16_t line565[65];
    png.getLineAsRGB565(pDraw, line565, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
    for(int i=0; i<64; i++) {
        // code from stackoverflow. I'm not very proud of myself
        rgb24 pixelColor = (rgb24){((((line565[i] >> 11) & 0x1F) * 527) + 23) >> 6, ((((line565[i] >> 5) & 0x3F) * 259) + 33) >> 6, (((line565[i] & 0x1F) * 527) + 23) >> 6};
        backgroundLayer.drawPixel(i, pDraw->y, pixelColor);
        backgroundLayer.drawPixel(i, 32 + pDraw->y, pixelColor);
    }
}

// Setup function
void setup() {
    // Debug serial
    Serial.begin(9600);

    // Bluetooth command serial
    Serial5.begin(9600);
    // pin is 1234

    // Initialize matrix
    matrix.addLayer(&backgroundLayer);
    matrix.begin();

    // Matrix is too bright, might as well dim it
    matrix.setBrightness(191);

    // Fill background with a nice gray
    backgroundLayer.fillScreen({24, 24, 24});

    // Display loading message
    backgroundLayer.setFont(font6x10);
    const char *loadingText = "LOADING";
    drawTextOnRGB24BackgroundLayer(backgroundLayer, 6, 11, 11, faceColor, loadingText);

    // Actually apply changes to matrix
    backgroundLayer.swapBuffers(false);

    // Begin SD
    SD.begin(BUILTIN_SDCARD);

    // Read video information
    readVideoConfigFromJSON("boykisser.json");
    Serial.println(video_filename_prefix);
    Serial.println(frame_cnt);

    // Initialize Serial1 in order to communicate with Raspberry Pi Zero 2 W 
    Serial1.begin(9600);

    // Set flag
    initialized = true;
}

// Loop function
void loop() {
    uint32_t startTime = millis();

    // Variables for input processing
    char receivedData[dataLength];
    bool newData = false;

    // Check for new bluetooth input and process it
    // no corruption check. I trust you bluetooth
    if(Serial5.available()) {
        delay(100);
        char command = Serial5.read();
        switch(command) {
            case 'v': // video change command
            {
                int btDataLength = Serial5.available();
                Serial.println(btDataLength);
                char* btReceivedData = (char*)malloc((btDataLength + 1) * sizeof(char));
                for(int i = 0; i < btDataLength; i++) { 
                    btReceivedData[i] = Serial5.read();
                }
                btReceivedData[btDataLength] = '\0';
                Serial5.println(btReceivedData);
                readVideoConfigFromJSON(btReceivedData);
                free(btReceivedData);
                break;
            }
            case 'm': // mode change command
            {
                mode = Serial5.read() - int('0');
                break;
            }   
        }
        while(Serial5.available()) {
            Serial5.read();
        }
    }

    // Check for new raspberry pi input
    if(Serial1.available() > dataLength) {
        // There was some error (bytes shifted, etc..) while receiving data, discard all of it
        // TODO: salvage at least SOME of the data. ex: only retrieve the last 5 bytes
        while(Serial1.available()) {
            Serial1.read();
        }
    }
    else if(Serial1.available() == dataLength) {
        // Read and verify data
        for(int i = 0; Serial1.available(); i++) { // Middle condition is essentially same as i<dataLength but juuust for extra safety
            receivedData[i] = Serial1.read();
        }
        if(receivedData[0] == '\0' && receivedData[dataLength - 1] == '\0') { // Check first and last bytes of data
            // Data was probably received without any corruptions
            newData = true;
        }
        else {
            // Either the beginning or the end of the data was corrupted or shifted
            // This means that the data could not be parsed and used
            // Well, actually let's just do nothing (only comments here)
        }
    }
    switch(mode) {
        case 0: // Facial expression mode
            // Conditionally update data and screen
            if(newData) {
                // New data has been successfully received
                // Parse the received data and update them
                mouthWidth = receivedData[mouthWidthDataIndex];
                mouthHeight = receivedData[mouthHeightDataIndex];
                leftEyeEAR128 = receivedData[leftEyeDataIndex];
                rightEyeEAR128 = receivedData[rightEyeDataIndex];
                noseOffset = receivedData[noseDataIndex] - 64;

                // Now update the matrix
                // First fill background with a nice gray
                backgroundLayer.fillScreen({24, 24, 24});

                char *mouthWidthLabelText = (char*) malloc(sizeof(char) * 11);
                char *mouthHeightLabelText = (char*) malloc(sizeof(char) * 11);
                char *leftEyeEARLabelText = (char*) malloc(sizeof(char) * 11);
                char *rightEyeEARLabelText = (char*) malloc(sizeof(char) * 11);
                char *noseLabelText = (char*) malloc(sizeof(char) * 12);
                sprintf(mouthWidthLabelText, "M.W: %d", mouthWidth);
                sprintf(mouthHeightLabelText, "M.H: %d", mouthHeight);
                sprintf(leftEyeEARLabelText, "L.E: %d", leftEyeEAR128);
                sprintf(rightEyeEARLabelText, "R.E: %d", rightEyeEAR128);
                sprintf(noseLabelText, "N.O: %d", noseOffset);
                backgroundLayer.setFont(font3x5);
                drawTextOnRGB24BackgroundLayer(backgroundLayer, 3, 0, 1, faceColor, mouthWidthLabelText);
                drawTextOnRGB24BackgroundLayer(backgroundLayer, 3, 0, 7, faceColor, mouthHeightLabelText);
                drawTextOnRGB24BackgroundLayer(backgroundLayer, 3, 0, 14, faceColor, leftEyeEARLabelText);
                drawTextOnRGB24BackgroundLayer(backgroundLayer, 3, 0, 20, faceColor, rightEyeEARLabelText);
                drawTextOnRGB24BackgroundLayer(backgroundLayer, 3, 0, 26, faceColor, noseLabelText);

                // Update actual display and discard all the drawing history stored in the buffer
                backgroundLayer.swapBuffers(false);

                // Free all allocated pointers
                free(mouthWidthLabelText);
                free(mouthHeightLabelText);
                free(leftEyeEARLabelText);
                free(rightEyeEARLabelText);
                free(noseLabelText);
            }

            // Basic loop control (sum.mit.edu)
            while(millis() - startTime < millisInOneLoop) {
                // Do nothing and wait
            }
            break;
        case 1: // Video display mode
            // Open frame image file
            Serial.println("OK");
            char* filename = (char*) malloc(sizeof(char) * (strlen(video_filename_prefix) + frame_idx_digits + strlen(video_filename_extension) + 1));
            char* sprintf_str = (char*) malloc(sizeof(char) * (2 + 4 + 2 + 1));
            sprintf(sprintf_str, "%%s%%0%dd%%s", frame_idx_digits);
            sprintf(filename, sprintf_str, video_filename_prefix, video_idx + frame_start, video_filename_extension);
            free(sprintf_str);

            // Open PNG
            backgroundLayer.fillScreen({0, 0, 0});
            Serial.println(filename);
            int rc = png.open(filename, openPNG, closePNG, readPNG, seekPNG, drawPNG);
            Serial.println(rc == PNG_SUCCESS);
            if(rc == PNG_SUCCESS){
                rc = png.decode(NULL, 0);
            }
            backgroundLayer.swapBuffers(false);
            png.close();
            free(filename);

            // Increment index and loop
            video_idx++;
            if(video_idx >= frame_cnt) {
                video_idx = 0;
            }

            while(millis() - startTime < (1000 / fps)) {
                // wait
            }
            break;
    }
}