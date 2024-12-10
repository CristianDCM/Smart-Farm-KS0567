#include <Arduino.h>
#include <WiFi.h>              // Librería para manejar la conexión WiFi en el ESP32
#include <AsyncTCP.h>          // Librería para manejar conexiones TCP de manera asincrónica 
#include <ESPAsyncWebServer.h> // Librería para configurar un servidor web asincrónico
#include <LiquidCrystal_I2C.h> // Librería para manejar pantallas LCD con interfaz I2C
#include <dht11.h>             // Librería para el sensor de temperatura y humedad DHT11
#include <Servo.h>             // Librería para controlar servomotores

// Definición de pines de los componentes
#define DHT11PIN 17        // Pin del sensor de temperatura y humedad
#define LEDPIN 27          // Pin del LED
#define SERVOPIN 26        // Pin del servomotor
#define FANPIN1 19         // Pin de entrada del ventilador (IN+)
#define FANPIN2 18         // Pin de entrada del ventilador (IN-)
#define STEAMPIN 35        // Pin del sensor de vapor
#define LIGHTPIN 34        // Pin del fotorresistor (sensor de luz)
#define SOILHUMIDITYPIN 32 // Pin del sensor de humedad del suelo
#define WATERLEVELPIN 33   // Pin del sensor de nivel de agua
#define RELAYPIN 25        // Pin del relé

// Definición de objetos
dht11 DHT11; 
LiquidCrystal_I2C lcd(0x27, 16, 2); // Pantalla LCD I2C (dirección 0x27)
Servo myservo;                       // Objeto para controlar el servomotor

// Credenciales de WiFi
const char* ssid = "SSID";     // Nombre de la red Wi-Fi
const char* password = "PASSWORD"; // Contraseña de la red Wi-Fi

// Variables de control para los dispositivos
static int A = 0; // Control del LED
static int B = 0; // Control del ventilador
static int C = 0; // Control del servomotor (alimentación)

// Crear objeto servidor web en el puerto 80
AsyncWebServer server(80);

// Página web HTML para control del sistema
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <title>Control Granja Inteligente</title>
    <meta charset="utf-8">
    <style>
        html, body {margin: 0; width: 100%; height: 100%;}
        body {
            display: flex; 
            justify-content: center; 
            align-items: center;
        }
        #dht {
            text-align: center; 
            width: 100%; 
            height: 100%; 
            color: #fff; 
            background-color: #47a047; 
            font-size: 48px;
        }
        .btn button {
            width: 100%; 
            height: 100%; 
            border: none; 
            font-size: 30px;
            color: #fff; 
            position: relative;
        }
        button {
            color: #ffff; 
            background-color: #89e689; 
            margin-top: 20px;
        }
        .btn button:active {top: 2px;}
    </style>
</head>
<body>
    <div class="btn">
        <div id="dht"></div>
        <button id="btn-led" onclick="setLED()">LED</button>
        <button id="btn-fan" onclick="setFan()">Ventilador</button>
        <button id="btn-feeding" onclick="setFeeding()">Alimentación</button>
        <button id="btn-watering" onclick="setWatering()">Riego</button>
    </div>

    <script>
        // Funciones para enviar comandos al servidor
        function setLED() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/set?value=A", true);
            xhr.send();
        }
        
        function setFan() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/set?value=B", true);
            xhr.send();
        }
        
        function setFeeding() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/set?value=C", true);
            xhr.send();
        }
        
        function setWatering() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/set?value=D", true);
            xhr.send();
        }
        
        // Actualizar datos de sensores cada segundo
        setInterval(function() {
            var xhttp = new XMLHttpRequest();
            xhttp.onreadystatechange = function() {
                if (this.readyState == 4 && this.status == 200) {
                    document.getElementById("dht").innerHTML = this.responseText;
                }
            };
            xhttp.open("GET", "/dht", true);
            xhttp.send();
        }, 1000);
    </script>
</body>
</html>
)rawliteral";

// Función para recopilar y formatear los datos de los sensores
String Merge_Data() {
    String dataBuffer;
    int chk = DHT11.read(DHT11PIN); // Leer datos del sensor DHT11

    // Obtener lecturas de los diferentes sensores
    String Steam = String(analogRead(STEAMPIN) / 4095.0 * 100);
    String Light = String(analogRead(LIGHTPIN));
    
    // Calcular humedad del suelo
    int shvalue = analogRead(SOILHUMIDITYPIN) / 4095.0 * 100 * 2.3;
    shvalue = shvalue > 100 ? 100 : shvalue;
    String SoilHumidity = String(shvalue);
    
    // Calcular nivel de agua
    int wlvalue = analogRead(WATERLEVELPIN) / 4095.0 * 100 * 2.5;
    wlvalue = wlvalue > 100 ? 100 : wlvalue;
    String WaterLevel = String(wlvalue);
    
    // Obtener temperatura y humedad
    String Temperature = String(DHT11.temperature);
    String Humidity = String(DHT11.humidity);

    // Formatear los datos para la página web
    dataBuffer += "<p><h1>Datos de Sensores</h1>";
    dataBuffer += "<b>Temperatura:</b><b>" + Temperature + "</b><b> °C</b><br/>";
    dataBuffer += "<b>Humedad:</b><b>" + Humidity + "</b><b> %rh</b><br/>";
    dataBuffer += "<b>Nivel de Agua:</b><b>" + WaterLevel + "</b><b> %</b><br/>";
    dataBuffer += "<b>Vapor:</b><b>" + Steam + "</b><b>%</b><br/>";
    dataBuffer += "<b>Luz:</b><b>" + Light + "</b><br/>";
    dataBuffer += "<b>Humedad del Suelo:</b><b>" + SoilHumidity + "</b><b> %</b><br/></p>";
    
    return dataBuffer;
}

// Función para procesar configuraciones recibidas desde la web
void Config_Callback(AsyncWebServerRequest *request) {
    if (request->hasParam("value")) {
        String HTTP_Payload = request->getParam("value")->value();
        Serial.printf("[%lu]%s\r\n", millis(), HTTP_Payload.c_str());

        // Control del LED
        if (HTTP_Payload == "A") {
            A = !A;
            digitalWrite(LEDPIN, A ? HIGH : LOW);
        }
        
        // Control del ventilador
        if (HTTP_Payload == "B") {
            B = !B;
            if (B) {
                digitalWrite(FANPIN1, HIGH);
                digitalWrite(FANPIN2, LOW);
            } else {
                digitalWrite(FANPIN1, LOW);
                digitalWrite(FANPIN2, LOW);
            }
        }
        
        // Control del servomotor (alimentación)
        if (HTTP_Payload == "C") {
            C = !C;
            myservo.write(C ? 180 : 80);
            delay(500);
        }
        
        // Control del riego
        if (HTTP_Payload == "D") {
            digitalWrite(RELAYPIN, HIGH);
            delay(400);
            digitalWrite(RELAYPIN, LOW);
            delay(650);
        }
    }
    request->send(200, "text/plain", "OK");
}

// Función para manejar rutas no encontradas
void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "No encontrado");
}

void setup() {
    Serial.begin(9600);

    // Conectar a la red WiFi
    WiFi.begin(ssid, password);
    while (!WiFi.isConnected()) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi conectado.");
    Serial.println("Dirección IP: ");
    Serial.println(WiFi.localIP());

    // Configurar pines de los componentes
    pinMode(LEDPIN, OUTPUT);
    pinMode(STEAMPIN, INPUT);
    pinMode(LIGHTPIN, INPUT);
    pinMode(SOILHUMIDITYPIN, INPUT);
    pinMode(WATERLEVELPIN, INPUT);
    pinMode(RELAYPIN, OUTPUT);
    pinMode(FANPIN1, OUTPUT);
    pinMode(FANPIN2, OUTPUT);

    // Configurar servomotor
    myservo.attach(SERVOPIN); 

    // Configurar pantalla LCD
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IP:");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());

    // Configurar rutas del servidor web
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", index_html);
    });
    server.on("/set", HTTP_GET, Config_Callback); 
    server.on("/dht", HTTP_GET, [](AsyncWebServerRequest *request) {
        String sensorData = Merge_Data();
        request->send(200, "text/html", sensorData);
    });
    server.onNotFound(notFound); 

    // Iniciar servidor web
    server.begin(); 
}

void loop() {
    // El servidor web maneja las peticiones de manera asincrónica
}