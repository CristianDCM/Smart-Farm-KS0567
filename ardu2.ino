#include <Arduino.h>
#include <WiFi.h>              // Librería para manejar la conexión WiFi en el ESP32
#include <AsyncTCP.h>          // Librería para manejar conexiones TCP de manera asincrónica 
#include <ESPAsyncWebServer.h> // Librería para configurar un servidor web asincrónico
#include <LiquidCrystal_I2C.h> // Librería para manejar pantallas LCD con interfaz I2C
#include <dht11.h>             // Librería para el sensor de temperatura y humedad DHT11
#include <Servo.h>             // Librería para controlar servomotores

// Definición de pines de los componentes
#define PIN_DHT11 17        // Pin del sensor de temperatura y humedad
#define PIN_LED 27          // Pin del LED
#define PIN_SERVO 26        // Pin del servomotor
#define PIN_VENTILADOR1 19  // Pin de entrada del ventilador (IN+)
#define PIN_VENTILADOR2 18  // Pin de entrada del ventilador (IN-)
#define PIN_VAPOR 35        // Pin del sensor de vapor
#define PIN_LUZ 34          // Pin del fotorresistor (sensor de luz)
#define PIN_HUMEDAD_SUELO 32 // Pin del sensor de humedad del suelo
#define PIN_NIVEL_AGUA 33   // Pin del sensor de nivel de agua
#define PIN_RELE 25         // Pin del relé

// Definición de objetos
dht11 SensorDHT11; 
LiquidCrystal_I2C pantalla(0x27, 16, 2); // Pantalla LCD I2C (dirección 0x27)
Servo servomotor;                        // Objeto para controlar el servomotor

// Credenciales de WiFi
const char* ssid = "SSID";         // Nombre de la red Wi-Fi
const char* contrasena = "PASSWORD"; // Contraseña de la red Wi-Fi

// Variables de control para los dispositivos
static int EstadoLED = 0;          // Control del LED
static int EstadoVentilador = 0;   // Control del ventilador
static int EstadoAlimentacion = 0; // Control del servomotor (alimentación)

// Crear objeto servidor web en el puerto 80
AsyncWebServer servidor(80);

// Página web HTML para control del sistema
const char pagina_html[] PROGMEM = R"rawliteral(
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
        <button id="btn-ventilador" onclick="setVentilador()">Ventilador</button>
        <button id="btn-alimentacion" onclick="setAlimentacion()">Alimentación</button>
        <button id="btn-riego" onclick="setRiego()">Riego</button>
    </div>

    <script>
        // Funciones para enviar comandos al servidor
        function setLED() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/set?value=LED", true);
            xhr.send();
        }
        
        function setVentilador() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/set?value=VENTILADOR", true);
            xhr.send();
        }
        
        function setAlimentacion() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/set?value=ALIMENTACION", true);
            xhr.send();
        }
        
        function setRiego() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/set?value=RIEGO", true);
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
String FusionarDatos() {
    String datosBuffer;
    int chk = SensorDHT11.read(PIN_DHT11); // Leer datos del sensor DHT11

    // Obtener lecturas de los diferentes sensores
    String Vapor = String(analogRead(PIN_VAPOR) / 4095.0 * 100);
    String Luz = String(analogRead(PIN_LUZ));
    
    // Calcular humedad del suelo
    int valorHumedadSuelo = analogRead(PIN_HUMEDAD_SUELO) / 4095.0 * 100 * 2.3;
    valorHumedadSuelo = valorHumedadSuelo > 100 ? 100 : valorHumedadSuelo;
    String HumedadSuelo = String(valorHumedadSuelo);
    
    // Calcular nivel de agua
    int valorNivelAgua = analogRead(PIN_NIVEL_AGUA) / 4095.0 * 100 * 2.5;
    valorNivelAgua = valorNivelAgua > 100 ? 100 : valorNivelAgua;
    String NivelAgua = String(valorNivelAgua);
    
    // Obtener temperatura y humedad
    String Temperatura = String(SensorDHT11.temperature);
    String Humedad = String(SensorDHT11.humidity);

    // Formatear los datos para la página web
    datosBuffer += "<p><h1>Datos de Sensores</h1>";
    datosBuffer += "<b>Temperatura:</b><b>" + Temperatura + "</b><b> °C</b><br/>";
    datosBuffer += "<b>Humedad del Aire:</b><b>" + Humedad + "</b><b> %rh</b><br/>";
    datosBuffer += "<b>Nivel de Agua:</b><b>" + NivelAgua + "</b><b> %</b><br/>";
    datosBuffer += "<b>Vapor:</b><b>" + Vapor + "</b><b>%</b><br/>";
    datosBuffer += "<b>Luz:</b><b>" + Luz + "</b><br/>";
    datosBuffer += "<b>Humedad del Suelo:</b><b>" + HumedadSuelo + "</b><b> %</b><br/></p>";
    
    return datosBuffer;
}

// Función para procesar configuraciones recibidas desde la web
void Configuracion_Callback(AsyncWebServerRequest *request) {
    if (request->hasParam("value")) {
        String CargaHTTP = request->getParam("value")->value();
        Serial.printf("[%lu]%s\r\n", millis(), CargaHTTP.c_str());

        // Control del LED
        if (CargaHTTP == "LED") {
            EstadoLED = !EstadoLED;
            digitalWrite(PIN_LED, EstadoLED ? HIGH : LOW);
        }
        
        // Control del ventilador
        if (CargaHTTP == "VENTILADOR") {
            EstadoVentilador = !EstadoVentilador;
            if (EstadoVentilador) {
                digitalWrite(PIN_VENTILADOR1, HIGH);
                digitalWrite(PIN_VENTILADOR2, LOW);
            } else {
                digitalWrite(PIN_VENTILADOR1, LOW);
                digitalWrite(PIN_VENTILADOR2, LOW);
            }
        }
        
        // Control del servomotor (alimentación)
        if (CargaHTTP == "ALIMENTACION") {
            EstadoAlimentacion = !EstadoAlimentacion;
            servomotor.write(EstadoAlimentacion ? 180 : 80);
            delay(500);
        }
        
        // Control del riego
        if (CargaHTTP == "RIEGO") {
            digitalWrite(PIN_RELE, HIGH);
            delay(400);
            digitalWrite(PIN_RELE, LOW);
            delay(650);
        }
    }
    request->send(200, "text/plain", "OK");
}

// Función para manejar rutas no encontradas
void noEncontrado(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "No encontrado");
}

void setup() {
    Serial.begin(9600);

    // Conectar a la red WiFi
    WiFi.begin(ssid, contrasena);
    while (!WiFi.isConnected()) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi conectado.");
    Serial.println("Dirección IP: ");
    Serial.println(WiFi.localIP());

    // Configurar pines de los componentes
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_VAPOR, INPUT);
    pinMode(PIN_LUZ, INPUT);
    pinMode(PIN_HUMEDAD_SUELO, INPUT);
    pinMode(PIN_NIVEL_AGUA, INPUT);
    pinMode(PIN_RELE, OUTPUT);
    pinMode(PIN_VENTILADOR1, OUTPUT);
    pinMode(PIN_VENTILADOR2, OUTPUT);

    // Configurar servomotor
    servomotor.attach(PIN_SERVO); 

    // Configurar pantalla LCD
    pantalla.init();
    pantalla.backlight();
    pantalla.clear();
    pantalla.setCursor(0, 0);
    pantalla.print("IP:");
    pantalla.setCursor(0, 1);
    pantalla.print(WiFi.localIP());

    // Configurar rutas del servidor web
    servidor.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", pagina_html);
    });
    servidor.on("/set", HTTP_GET, Configuracion_Callback); 
    servidor.on("/dht", HTTP_GET, [](AsyncWebServerRequest *request) {
        String datosSensores = FusionarDatos();
        request->send(200, "text/html", datosSensores);
    });
    servidor.onNotFound(noEncontrado); 

    // Iniciar servidor web
    servidor.begin(); 
}

void loop() {
    // El servidor web maneja las peticiones de manera asincrónica
}