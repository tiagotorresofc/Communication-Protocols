#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Definições de pinos para o RFID
#define SS_PIN 21
#define RST_PIN 22
MFRC522 mfrc522(SS_PIN, RST_PIN); // Instância MFRC522

#define MAX_UIDS 10
String authorizedUIDs[MAX_UIDS]; // Array para armazenar UIDs autorizados
int uidCount = 0;

// Credenciais Wi-Fi e configurações MQTT
const char* ssid = "";            // Substitua pelo seu SSID Wi-Fi
const char* password = "";  // Substitua pela sua senha Wi-Fi
const char* mqtt_server = ""; // Substitua pelo endereço IP do seu servidor MQTT (Linux)
const char* mqtt_topic = "rfid/sensor";
const char* mqtt_commands_topic = "rfid/commands";

WiFiClient espClient;
PubSubClient client(espClient);

// Função de configuração inicial
void setup() {
  Serial.begin(9600);  // Inicializa a comunicação serial
  SPI.begin();         // Inicializa a comunicação SPI
  mfrc522.PCD_Init();  // Inicializa o módulo MFRC522

  setup_wifi();        // Conecta ao Wi-Fi
  client.setServer(mqtt_server, 1883); // Configura o servidor MQTT
  client.setCallback(callback); // Define a função de callback para lidar com mensagens recebidas

  Serial.println("Aproxime o seu cartão do leitor...");
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conectar ao MQTT...");
    if (client.connect("ArduinoClient")) {
      Serial.println("Conectado");
      client.subscribe(mqtt_commands_topic); // Subscreve ao tópico de comandos
      Serial.println("Subscrito ao tópico de comandos MQTT");
    } else {
      Serial.print("Falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Verifica se há um novo cartão presente
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Leitura do UID do cartão
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  content.toUpperCase();

  // Exibe o UID do cartão no monitor serial
  Serial.print("Cartão detectado, UID: ");
  Serial.println(content);

  // Verifica se o UID é autorizado
  if (isAuthorized(content)) {
    Serial.println("Acesso autorizado");
    client.publish(mqtt_topic, "Acesso autorizado");
  } else {
    Serial.println("Acesso negado");
    client.publish(mqtt_topic, "Acesso negado");
  }
  delay(3000);
}

void callback(char* topic, byte* message, unsigned int length) {
  String command = "";
  for (int i = 0; i < length; i++) {
    command += (char)message[i];
  }

  Serial.print("Mensagem recebida [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(command);

  if (command.startsWith("ADD ")) {
    String newUID = command.substring(4);
    addUID(newUID);
  } else if (command == "LIST") {
    listUIDs();
  } else if (command == "CLEAR") {
    clearUIDs();
  } else {
    Serial.println("Comando desconhecido.");
  }
}

void listUIDs() {
  String uids = "UIDs autorizados:";
  for (int i = 0; i < uidCount; i++) {
    uids += "\n" + authorizedUIDs[i];
  }
  if (uidCount == 0) {
    uids += "\nNenhum UID autorizado.";
  }
  client.publish(mqtt_topic, uids.c_str());
  Serial.println(uids);
}

void addUID(String uid) {
  if (uidCount < MAX_UIDS) {
    authorizedUIDs[uidCount] = uid;
    uidCount++;
    String message = "UID adicionado: " + uid;
    client.publish(mqtt_topic, message.c_str());
    Serial.println(message);
  } else {
    client.publish(mqtt_topic, "Erro: Número máximo de UIDs atingido.");
    Serial.println("Erro: Número máximo de UIDs atingido.");
  }
}

void clearUIDs() {
  uidCount = 0;
  client.publish(mqtt_topic, "Todos os UIDs foram removidos.");
  Serial.println("Todos os UIDs foram removidos.");
}

bool isAuthorized(String uid) {
  for (int i = 0; i < uidCount; i++) {
    if (authorizedUIDs[i] == uid) {
      return true;
    }
  }
  return false;
}
