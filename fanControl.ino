#include <Scheduler.h> // thread gibi çalışıyor aynı anda birden fazla işlemi yapmamızı sağlayan bir kütüphane 
#include <Task.h> // schedular'ın alt kütüphanesi 
#include <ESP8266WiFi.h> // wifi a bağlanılmasını sağlayan arduino kütüphanesi 
#include <PubSubClient.h> // mqtt ye bağlanmasını sağlayan kütüphane 
#include <EEPROM.h> //konfigurasyonu eeproma kaydetmek için

#define indicatorLED    4 // gpio4 e bağlı olan ledi yakıyor kırmızı led D3 
#define fanPin		    5 // gpio5 fan bağlı D2

char ssid[10] = "tunahan"; // ssid bağlanılan wifi ismi 
char password[10] = "bruhhhhh"; // wifi şifresi
// arayüzden gelen mesaja göre esp, mqtt ile haberleşip sunucu üzerinden arayüz ve esp haberleşmesi sağlanıyor 
const char* mqttServer = "tuna.sh"; // sunucunun adresi
const int mqttPort = 1884; // mqtt nin portu 
const char* mqttUser = ""; // eğer user varsda girilir 
const char* mqttPassword = ""; // eğer password varsa girilir 

volatile int isFirstEverWifi = 1;



int minTemp = 20; //default olarak 20 derece üstünde fan calisacak (sadece oto mod için) iki mod var otomatik mod -> 20 derecenin üstündeyse oto çalışıyor ve arayüz mod -> arayüzü kontrol eden kişi belirler
int signal = 0; // fanın açık mı kapalı mı durumunu kontrol eder 
int speed = 0; // fanın hızını ayarlar 255 maks hızı, 0 iken çalışmıyor 
int autoMode = 1; // bu high iken otomatik modda yani 20 derece üstüne çıkınca çalışıyor 
int therm = 0; // sıcaklık değeri 

WiFiClient espClient; // WifiClient classının objesi espClient ,wifi bilgilerini içerir
PubSubClient client(espClient); // PubSubClient classının client objesinin escClient argümanı , mqttye bağlanılmasını sağlar

String split(String data, char delimiter, int index) { //Stringi belirli parçalaraya aırmak için fonksiyon. javadaki splitle aynı
  int start = 0;
  int end = -1;
  int currentIndex = 0;

  while (currentIndex <= index) {
    start = end + 1;
    end = data.indexOf(delimiter, start);
    if (end == -1) {  
      end = data.length();
    }

    if (currentIndex == index) {
      return data.substring(start, end);
    }
    currentIndex++;
  }
  return "";  
}


static void mqttCallback(char* topic, byte* payload, unsigned  int length){ // abone olduğumuz mqtt topiclerinden mesaj geldiğinde tetiklenen Callback fonksiyonu  
// gelen mesaj byte cinsinden, bu mesajı bytedan chara çeviriyor -> byte ile işlem yapılmaz chara çevrilmesi gerekir 
//  unsigned  int length gelen verinin kaç byte olduğunu belirtir 
			char charedPayload[16] = {0}; // random değerler gelmesin diye 0 a eşitlendi 
			for (int i = 0; i < length; i++) { // byte'lar char'a çevirilip payload'a yazılır 
    				charedPayload[i] = (char)payload[i]; // type casting 
  			}
			// otomatik moddaysa bunlar çalışmaz 
			if (strcmp(topic, "esp/signal") == 0) { // strcmp fonksiyonu-> iki tane string parametresi alır ve eşit mi değil mi kontrol eder. topic ile esp signalı karşılaştırır
				signal = atoi(charedPayload); // eşit ise sinyale gelen sinyal sinyale eşitlenir 
				Serial.write("signal is " + signal); // gelen signal mesajı ekrana yazdırılır 1 veya 0 
				EEPROM.write(0,(byte)signal);
			}
			if (strcmp(topic, "esp/speed") == 0) {
				speed = atoi(charedPayload); 

				EEPROM.write(1,(byte)speed);
			}
			if (strcmp(topic, "esp/auto") == 0) {

				autoMode = atoi(charedPayload);
				Serial.write("Auto mod is " + autoMode);
				EEPROM.write(2,(byte)autoMode);
			}
			if (strcmp(topic, "esp/minTemp") == 0) { // minimum sıcaklık eğerini arayüzde değiştirirsem buraya giriyor ve sıcaklığı örnein 20 derece ayarlarsam min sıcalık 20 olur
				minTemp = atoi(charedPayload); // atoi ascii to integer demek gelen ascii değerlerini integera çevirir

				EEPROM.write(3,(byte)minTemp);
			}

			if (strcmp(topic, "esp/wifi") == 0) {
				split(charedPayload,';',0).toCharArray(ssid, sizeof(ssid));
				split(charedPayload, ';' , 1).toCharArray(password, sizeof(password));
				for(int i = 0; i < 10; i++){
					
					EEPROM.write(4+i,(byte)ssid[i]);
				} 

				for(int i = 0; i < 10; i++){
					
					EEPROM.write(14+i,(byte)password[i]);
				} 

				isFirstEverWifi = 0;
			}	

			if (strcmp(topic, "esp/statusCheck") == 0) { 
		
				char payload[100]; 
				snprintf(payload, sizeof(payload), "%d;%d;%d;%d;", signal, speed, autoMode, minTemp); // signal, speed, autoMode, minTemp durumlarını yazıyo 
				client.publish("esp/statusInfo", payload); // mesaj yollanılan topiclere abone olunması gerekmez. statusInfo topic'i verilerin hepsini arayüze yolluyor 
				Serial.println("status info gonderildi"); 
							
			}else if(strcmp(topic, "esp/speed") != 0){
				EEPROM.commit();
				Serial.println("EEPROM kaydı yapıldı");
			}
	    
			Serial.println("MQTT vpaketi alındı");

			
			
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
		    Serial.println( "IP : " + WiFi.localIP().toString()); 
		}
		
		void connectMQTT(){ // mqtt bağlantısının yapıldığı fonksiyon 
		
		    client.setServer(mqttServer, mqttPort); // sunucu bilgilerini alıp mqtt bağlantısını gerçekleştiren fonksiyon
		    client.setCallback(mqttCallback); // arayüzden mesaj geldğinde tetiklenecek fonksiyonu setlediğimiz metod 
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
			
			client.publish("esp/therm", buffer); // mqttnin therm topic'ine ntc değerlerini yolluyor, esp bağlandığında ekranda bu sıcaklık değeri yazar ve 
			                                     // ve grafik oluşturmaya başlar 500ms de bir bu analog veriyi alıp grafiği çizer 1
			delay(500);

		}


}thermReader;

class mqttReader : public Task{ // sürekli olarak topiclerden mesaj gelip gelmediğini kontrol eder
	public:
		void setup(){}
		void loop(){
			client.loop(); //topclerin sürekli dinlenmesini sağlar
		}

}mqttReader;


void setup(){ 
	Serial.begin(115200);

	EEPROM.begin(512);
  
	signal = (int)EEPROM.read(0);
	speed = (int)EEPROM.read(1);
	autoMode = (int)EEPROM.read(2);
	minTemp = (int)EEPROM.read(3);

	if(isFirstEverWifi == 0){
	
		for(int i = 0; i < 10; i++){
		
			ssid[i] = (char)EEPROM.read(4+i);

		}	
	
		for(int i = 0; i < 10; i++){
		
			password[i] = (char)EEPROM.read(14+i);

		}


	}
	// threadleri başlatıyor 
	Scheduler.start(&connectionTask); // wifi bağlanmak ve kontroletmek, mqtt bağlanır kontroş eder, mqtt kanallarına abone olur
	Scheduler.start(&fanControlTask); // fanın çalışıp çalışılmayacağı, hızı vs sıcaklık kontrol edilir
	Scheduler.start(&thermReader); // sıcaklık belirlenir
	Scheduler.start(&mqttReader); // mqtt mesajları dinlenir
	Scheduler.begin(); 

}
void loop(){}
