#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>
#include <esp_camera.h>

const char* WIFI_SSID = "esp";
const char* WIFI_PASS = "12345678";

WebServer server(80);

#define LED_PIN 4 

// Smanjena rezolucija streama na 320x240 za bolju stabilnost
static auto streamRes = esp32cam::Resolution::find(1024, 768);
static auto loRes    = esp32cam::Resolution::find(160, 120);
static auto midRes   = esp32cam::Resolution::find(320, 240);
static auto hiRes    = esp32cam::Resolution::find(640, 480);

void serveJpg() {
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }
  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(),
                static_cast<int>(frame->size()));

  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

void handleJpgLo() {
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }
  serveJpg();
}

void handleJpgHi() {
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  serveJpg();
}

void handleJpgMid() {
  if (!esp32cam::Camera.changeResolution(midRes)) {
    Serial.println("SET-MID-RES FAIL");
  }
  serveJpg();
}

void streamHandler() {
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  client.print(response);
  
  while (client.connected()) {
    auto frame = esp32cam::capture();
    if (frame == nullptr) {
      Serial.println("CAPTURE FAIL");
      break;
    }
    
    client.print("--frame\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.print("Content-Length: " + String(frame->size()) + "\r\n\r\n");
    frame->writeTo(client);
    client.print("\r\n");
    
    // Opcioni manji delay za bolji FPS
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  
  pinMode(LED_PIN, OUTPUT); // Inicijalizacija LED pina kao izlaz

  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(streamRes);  // Smanjena rezolucija za stream
    cfg.setBufferCount(5);         // Povećan broj bafera za stabilnost
    cfg.setJpeg(90);               // Manji kvalitet JPEG-a za brži streaming

    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.print("http://");
  Serial.println(WiFi.localIP());
  Serial.println("  /cam-lo.jpg");
  Serial.println("  /cam-hi.jpg");
  Serial.println("  /cam-mid.jpg");
  Serial.println("  /stream");

  server.on("/cam-lo.jpg", handleJpgLo);
  server.on("/cam-hi.jpg", handleJpgHi);
  server.on("/cam-mid.jpg", handleJpgMid);
  server.on("/stream", streamHandler);

  server.begin();
}

void loop() {
  server.handleClient();

  // Provera serijskih komandi
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim(); // Uklanja viškove belina i novi red

    if (command == "ON") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED ON");
    } 
    else if (command == "OFF") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED OFF");
    }
  }
}
