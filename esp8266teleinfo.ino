//https://geo80blog.wordpress.com/2017/02/25/recuperer-les-informations-du-compteur-edf-avec-un-esp8266/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
WiFiManager wifiManager;
#include <PubSubClient.h>
ESP8266WebServer httpServer(80);

const char* mqtt_server = "url.com"; //INDIQUER ICI L'URL DU BROKER
const char* ID = "ESP_teleinfo";
const char* willTopic= "ESP_teleinfo/status";

WiFiClient espClient;
PubSubClient client(espClient);

long readInterval=60*1000;

long lastRead = 60000;

String trame="init.";
void setup() {
	Serial.begin(1200,SERIAL_7E1); //config pour lire la TI
	wifiManager.setDebugOutput(false);
    wifiManager.autoConnect(ID);
    client.setServer(mqtt_server, 1883);

	httpServer.on("/", [](){httpServer.send(200, "text/html", "it's works!");Serial.println("http");});
	httpServer.on("/info", [](){displayInfo();Serial.println("httpinfo");});
	httpServer.begin();
	delay(1000);

}
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(ID,willTopic,0,true,"Offline")) { //connection avec un LWT
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop() {
	if (!client.connected()) {
		reconnect();
	}
	client.loop();
	httpServer.handleClient();
	if(millis()-lastRead > readInterval || millis()-lastRead < 0){
		boolean erreur=false;
		lastRead=millis();
		//Serial.println("reading..");
		trame="";
		//client.publish("maison/compteurEDF/debug","debut mesure");
		while(Serial.available()){Serial.read();} //Vide le buffer : indispensable !
		int i=0;
		while(Serial.read()!=0x02){//On attend le debut (et on arrete si ca dure plus de 5 secondes)
			client.loop();
			yield();
			if(millis() - lastRead > 5000){
					erreur=true;
					client.publish(willTopic,"Erreur compteur",true);
					trame="Erreur de lecteur sur le compteur";
					break;
			}
		}
		if(erreur==false){
			while(true){ //boucle pour récuperer le contenu du port serie
				while(!Serial.available()){client.loop();yield();} //On attent le buffer, (!)
				char c=Serial.read();
				if(c==0X03){break;}//fin de la trame
				else{
					trame+=c;
				}
				yield();
			}
			int start=-1;
			int stop=-1;
			i=0;
			if(trame.length()>0){
				while(true){
					start=trame.indexOf(0x0A,stop+1); //le stop précédent
					if(start==-1){break;} //fini !
					stop=trame.indexOf(0x0D,start+1);
					int sep1=trame.indexOf(0x20,start+1);
					int sep2=trame.indexOf(0x20,sep1+1);

					String libel=trame.substring(start+1,sep1);
					String val=trame.substring(sep1+1,sep2);
					String crc=trame.substring(sep2+1,stop);
					sendMQTT(libel,val); //if checksum ok
					i++;
					if(i>50){break;Serial.println("Erreur");}
					client.loop();
					yield();
				}
				client.publish(willTopic,"OK",true);
			}
		}

	}
}

String checksum(String libel, String val){
	String str=libel+' '+val;
	char sum=0;
	for (int i = 0 ; i<=str.length() ; i++){
		sum=sum+str[i];
	}
	sum=(sum & 0x3F)+0x20 ;//garder que les 6bits de fin
	String sumS = String(sum);
	return sumS;

}
void sendMQTT(String libel, String val){
	String topic = ID+String("/");
	topic += libel;
	
	char topicChar[50];
	char valChar[35];
	topic.toCharArray(topicChar,50);
	val.toCharArray(valChar,35);
	client.publish(topicChar,valChar);
}
void displayInfo(){
	String html="<pre>";
	html+=trame;
	html+="</pre>";
	httpServer.send(200, "text/html", html);
}
