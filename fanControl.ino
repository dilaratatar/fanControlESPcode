#include <Scheduler.h> // thread gibi çalışıyor aynı anda birden fazla işlemi yapmamızı sağlayan bir kütüphane 
#include <Task.h> // schedular'ın alt kütüphanesi 
#include <ESP8266WiFi.h> // wifi a bağlanılmasını sağlayan arduino kütüphanesi 
#include <PubSubClient.h> // mqtt ye bağlanmasını sağlayan kütüphane 

#define indicatorLED    4 // gpio4 e bağlı olan ledi yakıyor kırmızı led D3 
#define fanPin		      5 // gpio5 fan bağlı D2

const char* ssid = "Iphone"; // ssid bağlanılan wifi ismi 
const char* password = "11111111"; // wifi şifresi
// arayüzden gelen mesaja göre esp, mqtt ile haberleşip sunucu üzerinden arayüz ve esp haberleşmesi sağlanıyor 
const char* mqttServer = "tuna.sh"; // sunucunun adresi
const int mqttPort = 1884; // mqtt nin portu 
const char* mqttUser = ""; // eğer user varsda girilir 
const char* mqttPassword = ""; // eğer password varsa girilir 

int minTemp = 20; //default olarak 20 derece üstünde fan calisacak (sadece oto mod için) iki mod var otomatik mod -> 20 derecenin üstündeyse oto çalışıyor ve arayüz mod -> arayüzü kontrol eden kişi belirler
int signal = 0; // fanın açık mı kapalı mı durumunu kontrol eder 
int speed = 0; // fanın hızını ayarlar 255 maks hızı, 0 iken çalışmıyor 
int autoMode = 1; // bu high iken otomatik modda yani 20 derece üstüne çıkınca çalışıyor 
int therm = 0; // sıcaklık değeri 

WiFiClient espClient; // WifiClient classının objesi espClient 
PubSubClient client(espClient); // PubSubClient classının client objesinin escClient argümanı 

static void mqttCallback(char* topic, byte* payload, unsigned  int length){ // abone olduğumuz mqtt topiclerinden mesaj geldiğinde tetiklenen Callback fonksiyonu  
// gelen mesaj byte cinsinden, bu mesajı bytedan chara çeviriyor -> byte ile işlem yapılmaz chara çevrilmesi gerekir 
//  unsigned  int length gelen verinin kaç byte olduğunu belirtir 
			char charedPayload[16] = {0}; // random değerler gelmesin diye 0 a eşitlendi 
			for (int i = 0; i < length; i++) { // byte'lar char'a çevirilip payload'a yazılır 
    				charedPayload[i] = (char)payload[i]; // type casting 
  			}
			if (strcmp(topic, "esp/signal") == 0) { // strcmp-> iki tane string eşit mi değil mi kontrol eder. 
				signal = atoi(charedPayload); // eşit ise sinyale gelen sinyal sinyale eşitlenir 
			}
			if (strcmp(topic, "esp/speed") == 0) {
				speed = atoi(charedPayload); 
			}
			if (strcmp(topic, "esp/auto") == 0) {
				autoMode = atoi(charedPayload);
			}
			if (strcmp(topic, "esp/minTemp") == 0) {
				minTemp = atoi(charedPayload);
			}
			if (strcmp(topic, "esp/statusCheck") == 0) { 
		
				char payload[100]; 
				snprintf(payload, sizeof(payload), "%d;%d;%d;%d;", signal, speed, autoMode, minTemp); // signal, speed, autoMode, minTemp durumlarını yazıyo 
				client.publish("esp/statusInfo", payload); // mesaj yollanılan topiclere abone olunması gerekmez. statusInfo topic'i verilerin hepsini arayüze yolluyor 
				Serial.println("status info gonderildi"); 
							
			}
	    
    	Serial.println(charedPayload); // seri monitore değerleri yazıyor
			Serial.println(signal);
			Serial.println(speed);
			Serial.println(autoMode);

		}

class connectionTask : public Task{ // bağlantı işlemlerinin yapıldığı thread 

	public:
		void connectWifi(){ // wifi bağlantısının yapıldığı fonksiyon 

		    Serial.println("Wifi baglantisi kuruluryor..."); 

		    WiFi.begin(ssid, password); // wifi bağlanma işlemlerini başlatan fonksiyon 

		    
		    while (WiFi.status() != WL_CONNECTED) { // wifi'a bağlı mı değil mi kontrol ediyor 
          delay(1000);
          Serial.print("."); // her saniyede 1 nokta koyulur
		    }

		    Serial.println(); 
		    Serial.println("Wifi'ye baglanildi");
		    Serial.println( "IP : " + WiFi.localIP()); 
		}
		
		void connectMQTT(){ // mqtt bağlantısının yapıldığı fonksiyon 
		
		    client.setServer(mqttServer, mqttPort); // sunucu bilgilerini alıp mqtt bağlantısını gerçekleştiren fonksiyon
		    client.setCallback(mqttCallback);
		    Serial.print("MQTT baglantisi kuruluyor...");
		    
		    if (client.connect("ESP8266Client", mqttUser, mqttPassword)) { // esp sunucuya bağlandığında ESP8266Client yazar
		      Serial.println("MQTT baglandi");

		      client.subscribe("esp/signal"); // abone olunan topicler (gruplar) fan açık mı kapalı mı
		      client.subscribe("esp/speed"); // sliderdan gelen veri 0 ve 255 arasında oranlanıp sunucu ile mqtt üzerinden esp ile haberleşiyor (veri yolluyor)
		      client.subscribe("esp/auto");  // auto mod butonunun 0 veya 1 olmasına göre mqtt üzerinden esp ile haberleşiyor 
		      client.subscribe("esp/statusCheck"); // arayüz bu topic üzerinden mesaj yolladığında o anki sistem değerlerini (espnin durumunu kontrol ediyor) sunucu üzerinden arayüze yolluyor 
          // arayüzün en güncel durumda kalmasını sağlıyor (örneğin en son otomatik modda kapatıldıysa yeniden açıldığında yine otomatik modda olmasını sağlıyor)
		      client.subscribe("esp/minTemp"); // otomatik moda girdiğinde fanın çalışacağı min sıcaklığı belirliyor kullanıcı belirliyor 
		      
		    } else {
		      Serial.print("basarisiz"); 
		      Serial.println(client.state()); // client.state() hata kodu döndürür 
		    }
		}
		void setup(){ // thread başladığında bir kere çalışır

      // fonksiyonlar çağırılır
			connectWifi(); 
			connectMQTT();
		}
		void loop(){ // thread başladığında sonsuza kadar çalışır

			if(WiFi.status() != WL_CONNECTED){ // wifi bağlanıp bağlanmadığını 10 sn'de bir kontrol eder 
				Serial.println("WiFi baglantisi koptu! Tekrar baglaniyor..."); 
				connectWifi(); // bağlantı başarısızsa tekrardan bağlanmayı dener
			}else{
				Serial.println("Wifi baglantisi saglikli..."); 
			}
			if(!client.connected()){  // mqtt bağlanıp bağlanmadığını 10 sn'de bir kontrol eder 
				Serial.println("MQTT baglantisi koptu! Tekrar baglaniyor..."); 
				connectMQTT();
			}else{

				Serial.println("MQTT baglantisi saglikli...");

			}
			delay(10000);
		}


}connectionTask; 


class fanControlTask : public Task{

	public:
		void setup(){

			
			pinMode(indicatorLED, OUTPUT); // ledin pinini çıkış olarak ayarlıyor gpio 

		}
		void loop(){ 

			if(autoMode == 1){ // otomatik moddaysa
				if(therm >= minTemp){ 
					int speedCoef = therm-minTemp;
					speed = map(speedCoef, 0, 30-minTemp, 128 ,255); // otomatik modda ortam sıcaklığına göre fan hızını ayarlıyor 
					analogWrite(fanPin, speed); // pwm veriyor
					digitalWrite(indicatorLED, 1); // ledi yakıyor
				}else{
					digitalWrite(fanPin, 0); // oda sıcaklığı 20 den azza pwm vermiyor yani fan çalışmıyor
					digitalWrite(indicatorLED, 0); // led yanmıyor
				}

			}else{ // otomatik modda değilse 
				if(signal == 1){ // fan açma komutu gelip gelinmediğine bakar 
			
					analogWrite(fanPin, speed); // geldiyse sliderda ayarlanana göre pwm verir
					digitalWrite(indicatorLED, 1); // ledi yakıyor
				}else{
					digitalWrite(fanPin, 0); // gelmediyse fan çalışmaz
					digitalWrite(indicatorLED, 0); // led yanmıyor
				}
			}

		}


}fanControlTask;

class thermReader : public Task{
	public:

		void setup(){}
		void loop(){
			int adcData = analogRead(A0);

			// ntc hesapları 
			float a = 610.5, b = -0.1332, c = -162.5;
			float Rntc, Vntc, Temp;
			Vntc = ((float)adcData*3.3)/1023.0;
			Rntc = 10000.0 * ((3.3/Vntc) - 1);
			therm = a * pow(Rntc, b) + c;
		
			char buffer[16]; 
			itoa(therm, buffer, 10); // gelen integer ntc verilerini string'e çevirir mqtt'ye yollamak için. itoa-> integer to ascii
			
			client.publish("esp/therm", buffer); // mqttnin therm topic'ine ntc değerlerini yolluyor 
			delay(500);

		}


}thermReader;

class mqttReader : public Task{ // sürekli olarak topiclerden mesaj gelip gelmediğini kontrol eder
	public:
		void setup(){}
		void loop(){
			client.loop();
		}

}mqttReader;

void setup(){ 
	Serial.begin(115200);

  // threadleri başlatıyor 
	Scheduler.start(&connectionTask); 
	Scheduler.start(&fanControlTask);
	Scheduler.start(&thermReader);
	Scheduler.start(&mqttReader);
	Scheduler.begin(); 

}
void loop(){}
