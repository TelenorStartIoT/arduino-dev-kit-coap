/*
 * Arduino Dev-Kit CoAP Example Code.
 * 
 * The example sends CoAP data packets over LTE-M (Cat M1)
 * or NB-IoT network to Telenor Managed IoT Cloud (MIC).
 * 
 * For help and other code examples:
 * Telenor Start IoT, https://startiot.telenor.com/
 */

#include <MKRNB.h>

#include "coap.h"
#include "arduino_secrets.h"

// Enter sensitive data and other configurations in "arduino_secrets.h".
char     pinnumber[]     = SECRET_PINNUMBER;
char     gprs_apn[]      = SECRET_GPRS_APN;
uint32_t coap_ip         = SECRET_COAP_IP;
char     coap_endpoint[] = SECRET_COAP_ENDPOINT;
uint32_t coap_port       = SECRET_COAP_PORT;
uint32_t rat             = SECRET_RAT;
uint32_t cops            = SECRET_COPS;
bool     debug           = true;

NB        nbAccess(debug);
GPRS      gprsAccess;
NBUDP     Udp;
Coap      coap(Udp);
IPAddress iotgw_ip(coap_ip);

void setup() {
  // Open serial communication and wait for port to open
  Serial.begin(115200);
  while (!Serial);

  /**
   * Issue a series of AT commands to the modem.
   * 
   *    - Wait for it to get ready
   *    - Disconnect from any connected networks
   *    - Set RAT (NB-IoT or LTE-M)
   *    - Apply changes and save configuration
   *    - Turn radio on
   *    - Check attachment
   *    - Set operator
   */

  Serial.print("Waiting for modem to get ready...");
  MODEM.begin();
  while (!MODEM.noop());
  Serial.println("done.");

  Serial.print("Disconnecting from network...");
  MODEM.send("AT+COPS=2");
  MODEM.waitForResponse(2000);
  Serial.println("done.");

  Serial.print("Setting Radio Access Technology to NB-IoT or LTE-M (7 is for LTE-M and 8 is for NB-IoT)...");
  MODEM.sendf("AT+URAT=%d", rat);
  MODEM.waitForResponse(2000);
  Serial.println("done.");

  Serial.print("Applying changes and saving configuration...");
  MODEM.send("AT+CFUN=15");
  do {
    delay(1000);
    MODEM.noop();
  } while (MODEM.waitForResponse(1000) != 1);
  Serial.println("done.");

  Serial.print("Modem ready, turn radio on in order to configure it...");
  MODEM.send("AT+CFUN=1");
  MODEM.waitForResponse(2000);
  Serial.println("done.");

  Serial.print("Check attachment until CSQ RSSI indicator is less than 99...");
  String response;
  int status = 99;
  while (status > 98 && status > 0) {
    MODEM.send("AT+CSQ");
    MODEM.waitForResponse(2000);
    String sub = response.substring(6, 8);
    status = sub.toInt(); // Will return 0 if no valid number is found
    delay(1000);
  }
  Serial.println("done.");

  Serial.print("Set operator to Telenor...");
  MODEM.sendf("AT+COPS=1,2,\"%d\"", cops);
  MODEM.waitForResponse(4000);
  Serial.println("done.");

  // Initialize CoAP client
  coap.start();

  // Seed random number generator with noise from pin 0
  randomSeed(analogRead(0));
}

void loop() {
  // Check if connected and if not, reconnect
  if (nbAccess.status() != NB_READY || gprsAccess.status() != GPRS_READY) {
    connectNB();
  }

  Serial.print("Send packet to Telenor MIC...");
  sendPacket();
  Serial.println("done.");

  // Continue CoAP operations (handle ACK, send response)
  coap.loop();

  Serial.println("Wait 10s before sending again...");
  delay(10000);
}

bool connectNB() {
  Serial.print("Attempting to connect to the cellular network...");
  // Failed, retry
  while (nbAccess.begin(pinnumber, gprs_apn) != NB_READY || gprsAccess.attachGPRS() != GPRS_READY) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("done.");

  return true;
}

uint16_t sendPacket () {
  // Generate random simulated data
  float tmp = 20 + (random(0, 9) / 10);
  float hum = 60 + (random(0, 9) / 10);

  /**
   * Create a JSON payload.
   * You would typically compress the payload as much as possible here,
   * and then use the "uplink transform" in the Thing Type to unpack it later
   * once the packet has been received in Telenor MIC.
   */
  char buffer[100];
  uint32_t buf_size = snprintf(buffer, 100, "{\"tmp\":\"%.2f\",\"hum\":\"%.2f\",\"latlng\":\"59.898812,10.627212\"}", tmp, hum);

  // Send a CoAP POST message to Telenor IoT Gateway
  uint16_t msgid = coap.send(
    iotgw_ip,           // IP address
    coap_port,          // Port
    coap_endpoint,      // CoAP endpoint
    COAP_CON,           // CoAP packet type (con, noncon, ack, reset)
    COAP_POST,          // CoAP method
    NULL,               // CoAP token
    0,                  // CoAP token length
    (uint8_t *) buffer, // Message payload
    buf_size            // Message payload length
  );

  return msgid;
}
