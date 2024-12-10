#include <Arduino.h>
#include <WiFi.h>              // Librería para manejar la conexión WiFi en el ESP32
#include <AsyncTCP.h>          // Librería para manejar conexiones TCP de manera asincrónica
#include <ESPAsyncWebServer.h> // Librería para configurar un servidor web asincrónico
#include <LiquidCrystal_I2C.h> // Librería para manejar pantallas LCD con interfaz I2C
#include <dht11.h>             // Librería para el sensor de temperatura y humedad DHT11

// Definición de pines de los componentes
#define PIN_DHT11 17         // Pin del sensor de temperatura y humedad
#define PIN_LED 27           // Pin del LED
#define PIN_VENTILADOR1 19   // Pin de entrada del ventilador (IN+)
#define PIN_VENTILADOR2 18   // Pin de entrada del ventilador (IN-)
#define PIN_VAPOR 35         // Pin del sensor de vapor
#define PIN_LUZ 34           // Pin del fotorresistor (sensor de luz)
#define PIN_HUMEDAD_SUELO 32 // Pin del sensor de humedad del suelo
#define PIN_NIVEL_AGUA 33    // Pin del sensor de nivel de agua
#define PIN_RELE 25          // Pin del relé
#define BUZZER_PIN 16        // Pin del buzzer

// Notas de la melodía "Feliz Navidad"
int melody[] = {
    330, 330, 330, 330, 330, 330, 330, 349, 330, 294, 349, 330,
    330, 330, 330, 330, 330, 330, 349, 330, 294, 349, 330, 330,
    330, 330, 330, 330, 330, 349, 330, 294};
    
int durations[] = {
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4};

// Definición de objetos
dht11 SensorDHT11;
LiquidCrystal_I2C pantalla(0x27, 16, 2); // Pantalla LCD I2C (dirección 0x27)

// Credenciales de WiFi
const char *ssid = "DESKTOP-TFAMG6O"; // Nombre de la red Wi-Fi
const char *contrasena = "123456789"; // Contraseña de la red Wi-Fi

// Variables de control para los dispositivos
static int EstadoLED = 0;        // Control del LED
static int EstadoVentilador = 0; // Control del ventilador
static int EstadoMusica = 0;

// Crear objeto servidor web en el puerto 80
AsyncWebServer servidor(80);

// Página web HTML para control del sistema
const char pagina_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <title>Control Granja Inteligente</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-dark: #121212;
            --bg-darker: #1E1E1E;
            --accent-color: #4A90E2;
            --text-color: #E0E0E0;
            --button-hover: #3A7BD5;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        html, body {
            height: 100%;
            font-family: 'Inter', sans-serif;
            background-color: var(--bg-dark);
            color: var(--text-color);
        }

        .container {
            max-width: 600px;
            margin: 0 auto;
            padding: 20px;
            height: 100%;
            display: flex;
            flex-direction: column;
            justify-content: center;
        }

        #dht {
            background-color: var(--bg-darker);
            text-align: center;
            padding: 30px;
            border-radius: 12px;
            margin-bottom: 20px;
            font-size: 2.5rem;
            font-weight: 600;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }

        .btn-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 15px;
        }

        .btn button {
            width: 100%;
            padding: 15px;
            border: none;
            border-radius: 10px;
            background-color: var(--accent-color);
            color: white;
            font-size: 1.2rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s ease;
            text-transform: uppercase;
            letter-spacing: 1px;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 10px;
        }

        .btn button svg {
            width: 24px;
            height: 24px;
        }

        .btn button:hover {
            background-color: var(--button-hover);
            transform: translateY(-2px);
            box-shadow: 0 4px 6px rgba(0,0,0,0.2);
        }

        .btn button:active {
            transform: translateY(1px);
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
    </style>
</head>
<body>
    <div class="container">
        <div id="dht"></div>
        <div class="btn-grid btn">
            <button id="btn-led" onclick="setLED()">
                <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-lightbulb"><path d="M15 14c.2-1 .7-1.7 1.5-2.5 1-.9 1.5-2.2 1.5-3.5A6 6 0 0 0 6 8c0 1 .2 2.2 1.5 3.5.7.7 1.3 1.5 1.5 2.5"/><path d="M9 18h6"/><path d="M10 22h4"/></svg>
                LED
            </button>
            <button id="btn-ventilador" onclick="setVentilador()">
                <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-wind">
                    <path d="M17.7 7.7a2.5 2.5 0 1 1 1.8 4.3H2"/><path d="M9.6 4.6A2 2 0 1 1 11 8H2"/><path d="M12.6 19.4A2 2 0 1 0 14 16H2"/>
                </svg>
                Ventilador
            </button>
            <button id="btn-alimentacion" onclick="setAlimentacion()">
                <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-bowl">
                    <path d="M20.5 8.5l.28-1.4A2 2 0 0 0 18.82 5H5.18a2 2 0 0 0-1.94 2.09l.78 7.91a2 2 0 0 0 1.94 1.91h11.84a2 2 0 0 0 1.94-1.91l.54-5.5"/><path d="M10.3 14s.36-1.3 1.7-1.3 1.7 1.3 1.7 1.3"/>
                </svg>
                Alimentación
            </button>
            <button id="btn-riego" onclick="setRiego()">
                <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-droplet">
                    <path d="M12 22a7 7 0 0 0 7-7c0-2-1-3.9-3-5.74C14 6.54 12 4 12 4s-2 2.54-4 5.26C6 11.1 5 13 5 15a7 7 0 0 0 7 7z"/>
                </svg>
                Riego
            </button>
            <button id="btn-musica" onclick="setMusica()">
                <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-music">
                    <path d="M9 18V5l12-2v13"/><circle cx="6" cy="18" r="3"/><circle cx="18" cy="16" r="3"/>
                </svg>
                Música
            </button>
        </div>
    </div>

    <script>
        // Funciones para enviar comandos al servidor (sin cambios)
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

        function setMusica() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/set?value=MUSICA", true);
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
String FusionarDatos()
{
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
    datosBuffer += "<p><h2>Datos de Sensores</h2><br/>";
    datosBuffer += "<b>Temperatura:</b><b>" + Temperatura + "</b><b> °C</b><br/><hr>";
    datosBuffer += "<b>Humedad del Aire:</b><b>" + Humedad + "</b><b> %rh</b><br/><hr>";
    datosBuffer += "<b>Nivel de Agua:</b><b>" + NivelAgua + "</b><b> %</b><br/><hr>";
    datosBuffer += "<b>Vapor:</b><b>" + Vapor + "</b><b>%</b><br/><hr>";
    datosBuffer += "<b>Luz:</b><b>" + Luz + "</b><br/><hr>";
    datosBuffer += "<b>Humedad del Suelo:</b><b>" + HumedadSuelo + "</b><b> %</b><br/></p>";

    return datosBuffer;
}

void reproducirMelodia()
{
    for (int i = 0; i < 32; i++)
    {
        tone(BUZZER_PIN, melody[i], 1000 / durations[i]);
        delay(1000 / durations[i] * 1.3); // Pausa entre notas
        noTone(BUZZER_PIN);
    }
}

// Función para procesar configuraciones recibidas desde la web
void Configuracion_Callback(AsyncWebServerRequest *request)
{
    if (request->hasParam("value"))
    {
        String CargaHTTP = request->getParam("value")->value();
        Serial.printf("[%lu]%s\r\n", millis(), CargaHTTP.c_str());

        // Control del LED
        if (CargaHTTP == "LED")
        {
            EstadoLED = !EstadoLED;
            digitalWrite(PIN_LED, EstadoLED ? HIGH : LOW);
        }

        // Control del ventilador
        if (CargaHTTP == "VENTILADOR")
        {
            EstadoVentilador = !EstadoVentilador;
            if (EstadoVentilador)
            {
                digitalWrite(PIN_VENTILADOR1, HIGH);
                digitalWrite(PIN_VENTILADOR2, LOW);
            }
            else
            {
                digitalWrite(PIN_VENTILADOR1, LOW);
                digitalWrite(PIN_VENTILADOR2, LOW);
            }
        }

        // Control del riego
        if (CargaHTTP == "RIEGO")
        {
            digitalWrite(PIN_RELE, HIGH);
            delay(400);
            digitalWrite(PIN_RELE, LOW);
            delay(650);
        }

        // Control de música
        if (CargaHTTP == "MUSICA")
        {
            reproducirMelodia();
        }
    }
    request->send(200, "text/plain", "OK");
}

// Función para manejar rutas no encontradas
void noEncontrado(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "No encontrado");
}

void setup()
{
    Serial.begin(9600);

    // Conectar a la red WiFi
    WiFi.begin(ssid, contrasena);
    while (!WiFi.isConnected())
    {
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
    pinMode(BUZZER_PIN, OUTPUT);

    // Configurar pantalla LCD
    pantalla.init();
    pantalla.backlight();
    pantalla.clear();
    pantalla.setCursor(0, 0);
    pantalla.print("IP:");
    pantalla.setCursor(0, 1);
    pantalla.print(WiFi.localIP());

    // Configurar rutas del servidor web
    servidor.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                { request->send(200, "text/html", pagina_html); });
    servidor.on("/set", HTTP_GET, Configuracion_Callback);
    servidor.on("/dht", HTTP_GET, [](AsyncWebServerRequest *request)
                {
        String datosSensores = FusionarDatos();
        request->send(200, "text/html", datosSensores); });
    servidor.onNotFound(noEncontrado);

    // Iniciar servidor web
    servidor.begin();
}

void loop()
{
    // El servidor web maneja las peticiones de manera asincrónica
}