# Suzuki OBD Professional / J2534 Diagnostic Suite (BETA) — Güncel Durum ve Yapılan İşlemler

**Geliştirici & Baş Mühendis:** [Ayem Kaan Yıldırım — Automotive Systems & Energy Storage Engineer](https://kaanyildirim.com/)  
**Resmi Web Sitesi:** [https://kaanyildirim.com/](https://kaanyildirim.com/)  
**Son Güncelleme:** 10 Kasım 2026  
**Test Donanımı:** MongoosePro JLR (Drew Technologies PassThru DLL `monpj432.dll`)  
**Doğrulanan Araç:** Suzuki Grand Vitara 1.9 DDiS (JB419D / Bosch EDC16C39 / Renault F9Q 95 kW)  

---

> [!IMPORTANT]
> **Canlı Araç Doğrulama Özeti:**
> Bu yazılım, MongoosePro JLR J2534 arayüzü ile doğrudan araç OBD soketine bağlanarak **fiziksel olarak test edilmiş ve doğrulanmıştır**. Motor Beyni (Bosch EDC16C39 0x7E0 CAN), Gövde Kontrol Modülü (BCM 0x5A K-Line Pin 7) ve Fren/Stabilite Beyni (ABS/ESP 0x29 K-Line Pin 7) ile çift yönlü iletişim, canlı veri akışı ve arıza taraması canlı araçta teyit edilmiştir.

---

## 1. 10 Kasım 2026 İtibarıyla Tamamlanan Kritik Geliştirmeler

### A. K-Line 4-Baytlık Paketleme & BCM Konfigürasyonunun Canlı Okunması (Çığır Açan Başarı)
- **Sorun:** Suzuki SDT-II mimarisinde yer alan Pin 7 K-Line modülleri (BCM `0x5A`, ABS/ESP `0x29`, 4WD `0x21`, SRS `0x59`, IMMO `0x5D`, HVAC `0x99`), standart ISO 14230 3-baytlık başlıklara (`0x80|len`) yanıt vermiyordu.
- **Çözüm:** `RunKLineSession()` fonksiyonu Suzuki'nin katı **4-baytlık başlık formatına** (`0x80 [Target] 0xF1 [Length] [Payload...]`) uyarlandı ve oturumlar arasına minimum **350 ms Tidle (hat boşta kalma)** gecikmesi eklendi.
- **Canlı Araç Test Sonucu:** Grand Vitara K-Line BCM'inden (`0x5A`) 37 baytlık konfigürasyon EEPROM verisi başarıyla çekildi:
  ```text
  K-Line BCM (0x5A) 21 08 Read OK:
  61 08 00 00 00 00 00 00 00 00 FF FF 45 FF FF FF 99 47 00 00 FF FF 24 FF FF FF FF FF FF FF FF FF 15 00 02 20 E0
  ```

---

### B. Direksiyon Açı Sensörü (SAS / C1075) & ABS/ESP Teşhisi
1. **Canlı Veri Deşifresi:**
   - ABS/ESP beyninden (`0x29`) okunan 34 baytlık `21 00` canlı veri çerçevesi deşifre edildi:
     - Akü Gerilimi: `11.84 V - 12.10 V`
     - Yanal İvme (Lateral G): `0.00 G`
     - Savrulma Oranı (Yaw Rate): `0.0 °/s`
     - Direksiyon Açısı: `7.0°` (Direksiyon simidi sağa doğru 7 derece açıda durmaktadır)
     - **Sensör Durum Baytı (Offset 26):** Bit 6 kontrolü ile sensör elektriksel durum tespiti.
2. **Kalibrasyonun Reddedilmesi (NRC 0x23) ve ESP Lambasının Sönmeme Nedenleri:**
   - Canlı araç taramasında ABS/ESP beyninde tespit edilen engeller:
     - **Engel 1 — Tekerlek Hız Sensörü Arızası (`C1021`):** `Ön Sağ Tekerlek Hız Sensörü Sinyal Yok / Devre Açık`. Bosch 8.0 ESP sistemi, tekerlek hız sensörlerinden biri bile devre dışıyken ESP sistemini güvenlik gereği kilitler, kadranda sarı ESP lambasını sürekli yakar ve Direksiyon Açı Sensörü kalibrasyonunu `7F A5 23` ile reddeder.
     - **Engel 2 — Direksiyon Simidi Düz Değil:** Kalibrasyon için direksiyonun kadrana göre tam `0.0° ± 1.5°` düz olması zorunludur. Araçtaki mevcut açı `7.0°` dir.
     - **Engel 3 — Zemberek / Sigorta Durumu (`C1037 / U1126`):** Sensör soketi veya direksiyon zembereği (clock spring) kaynaklı besleme kesintisinde sensör açı verisi üretemez.
   - **Çözüm Algoritması:** `ExecuteSASCalibration()` fonksiyonu dinamikleştirildi; kalibrasyon reddedildiğinde ABS'deki aktif DTC'leri (`C1021`, `C1037` vb.) ve mevcut direksiyon açısını canlı okuyup kullanıcıya adım adım tam Türkçe arıza ve çözüm raporu sunar.

### B2. Şasi Numarası (VIN) Üst Durum Çubuğu (Topbar) Entegrasyonu
- Arayüzün en üst header/status çubuğuna (`.status-strip`) belirgin ve tıklanabilir bir **Şasi No / VIN gösterge rozeti** (`#topbarVin`) eklendi.
- VIN okuma fonksiyonu (`ExecuteVinActionJson`), Bosch EDC16C39 ve Suzuki CAN sistemleri için **`10 C0` oturum öncülü**, OBD-II Mode 09 PID 02, ve K-Line Pin 7 çoklu kontrol ünitesi (ECM `0x11/0x7A`, BCM `0x5A`, IMM `0x5D`) taraması ile güçlendirildi.
- Okunan 17 haneli şasi numarası `g_cachedVin` değişkeninde hafızaya alınarak `/api/status`, `/api/live` ve `/api/scan` üzerinden anında arayüze aktarılır.
- Üst çubuktaki Şasi No kutusuna tıklandığında VIN tek tıkla panoya (clipboard) kopyalanır.
3. **Arayüz İyileştirmeleri:**
   - **Tab 1 (DTC):** `C1075` kodu tespit edildiğinde doğrudan satır içine **`[🎯 SAS Kalibrasyonunu Başlat (ABS-03)]`** aksiyon butonu eklendi.
   - **Tab 2 (Canlı Veri):** Canlı veri tablosuna `Direksiyon Açısı (SAS)` (`°`), `Savrulma Hızı` (`°/s`), `Yanal İvme` (`G`) ve **`Direksiyon Sensörü Durumu`** parametresi eklendi. `21 00` bayt 26 bit 6 (`0x40`) sıfır olduğunda tek başına besleme/zemberek arızası kanıtı sayılamayacağı için arayüzde nötr `DURUM BİTİ PASİF / KALİBRE DEĞİL` rozeti gösterilir; `C1037` veya kablo kontrolü yalnızca aktif `C1037` DTC'si varsa vurgulanır.
   - **Hata Açıklamaları:** Kalibrasyon reddedildiğinde ekranda detaylı Türkçe teşhis ve çözüm rehberi penceresi çıkar.

---

### C. Arayüz Düzeni ve JSON Taşması Hatasının Giderilmesi
- **Sorun:** Otomatik algılama veya aday inceleme butonuna basıldığında, veritabanı model JSON verisinin (`userReviewOnly: true...`) başlığın altındaki `<pre id="modelDetails">` alanına dökülerek tüm sekmeleri ekranın altına ittiği ve arayüzü kilitlediği gözlemlendi.
- **Çözüm:** 
  - Model aday ve detay paneli tepe gezinti çubuğundan tamamen kaldırıldı.
  - **4. Sekme (ECU Kimlik & K-Line)** içerisine taşınarak 100px kaydırma çubuklu şık ve derli toplu bir teşhis kartına dönüştürüldü.
  - Arayüzün 100vh tek ekran standardı garanti altına alındı.

---

### D. Enjektör IMA Kodlama (DSL-01 / 21 AD) Oturum Sürekliliği (SendBatch)
- **Sorun:** Bosch EDC16C39 ECM'de enjektör kalibrasyon kodları okunurken `Could not read Injector IMA codes (21 AD / 21 54)` hatası alınıyordu.
- **Neden & Çözüm:** 
  - `DSL-01.js` standardına göre motor beyninde önce `10 C0` özel diagnostik oturumu açılmalı, ardından **aynı açık kanal üzerinden** `21 AD` komutu gönderilmelidir.
  - Eski kodda `10 C0` gönderilip J2534 kanalı kapatıldığı (`ptDisconnect`) için oturum düşüyordu.
  - `ExecuteInjectorImaJson()` fonksiyonu **`SendBatch`** altyapısına geçirildi. Tek oturumda kesintisiz `10 C0` -> `21 AD` okunması ve `3B AD FF` ile 4 silindirin 7 haneli IMA kodlarının yazılıp `3B 7E FE` ile ZFC sıfırlanması sağlandı.

---

### E. Cruise Control (Hız Sabitleyici) Analizi ve Çalışma Şartları
1. **Yazılımsal Durum:**
   - Grand Vitara 1.9 DDiS (Bosch EDC16C39) motor kontrol ünitesinde Cruise Control yazılımı **fabrikasyon olarak varsayılan olarak açıktır**. BCM'den veya bilgisayardan açılması gereken gizli bir "enable/disable" anahtarı yoktur.
2. **Çalışmama Nedenleri ve Çözüm Adımları:**
   - **ABS / ESP Lambası (En Büyük Engel):** Bosch EDC16C39 motor beyni, araçta aktif ABS/ESP tekerlek hız arızası (`C1021/C1022`) veya açı sensörü arızası (`C1037/C1075`) varken Cruise Control'ü güvenlik gereği anında devre dışı bırakır.
   - **Direksiyon Zembereği (Clock Spring / Spiral Kablo):** Açı sensörüne elektrik gitmesini engelleyen zemberek kopukluğu, aynı döner yay içerisinden geçen direksiyon simidi Cruise Control tuşlarının da (`CRUISE ON/OFF`, `RES/ACC`, `SET/COAST`) sinyal iletmesini engeller.
   - Direksiyon zembereği onarılıp ESP lambası söndürüldüğünde, Cruise Control **ekstra hiçbir kodlamaya gerek kalmadan kendiliğinden devreye girecektir.**

---

### F. Grand Vitara 1.9 DDiS BCM Gizli Özellik & Kişiselleştirme Kataloğu

Aracın BCM beyninden okunan gerçek `rawHex` ve SDT-II veritabanı (`BCM-06`, `BCM-103`, `3B`) temel alınarak Grand Vitara 1.9 DDiS profiline (`gv_jb419d`) entegre edilen fonksiyonlar:

| ID | Gizli Özellik / Fonksiyon | Komut / Servis | Seçenekler |
| :--- | :--- | :--- | :--- |
| **HF-ALARM** | Fabrika Güvenlik Alarmı (Security Alarm) | `30 09 08 20` / `3B 21` | AÇIK (`1`) / KAPALI (`0`) *(Araçta: AÇIK)* |
| **HF-ANSWERLIGHT** | Kilitleme Görsel Onayı | `30 10 08 80` | Dörtlü Sinyal (`0`) / Tavan Lambası (`1`) *(Araçta: Sinyal)* |
| **HF-DTRL** | Gündüz Sürüş Farları (DRL) | `BCM 3B 3F` | AÇIK (`02`) / KAPALI (`00`) |
| **HF-AUTOLOCK** | Hıza Duyarlı Otomatik Kapı Kilitleme (15 km/s) | `BCM 3B 2F` | Hıza Duyarlı (`01`) / KAPALI (`00`) |
| **HF-AUTOUNLOCK** | Kontak Kapatınca Otomatik Kilit Açma | `BCM 3B 02` | Kontak Kapatınca Aç (`01`) / KAPALI (`00`) |
| **HF-UNLOCKMODE** | Kumandadan Kilit Açma Davranışı | `BCM 3B 63` | Tek Basış Tüm Kapılar (`00`) / 2 Aşamalı (`01`) |
| **HF-AUTOLIGHT** | Otomatik Far Sensörü Donanım Kodlaması | `BCM 3B 3A` | Sensör Aktif (`04`) / Devre Dışı (`06`) |
| **HF-KEYLESS** | Keyless Anahtarsız Giriş & Çalıştırma | `BCM 3B 33` | Keyless Aktif (`00`) / Devre Dışı (`01`) |
| **HF-IKEY** | I-Key Akıllı Anahtar Sistemi | `BCM 3B 34` | I-Key Aktif (`01`) / Devre Dışı (`00`) |
| **HF-ROOMLAMP** | Tavan Lambası Sönme Gecikmesi (Dimmer) | `BCM 3B 16` | 0.5s / 1.0s / 2.0s / 3.0s / 0s |
| **HF-RETAINEDPWR** | Kontak Sonrası Cam Kaldırma Yetkisi Süresi | `BCM 3B 27` | Uzatılmış Süre / Standart / Kapalı |
| **HF-BUZZER** | Kapı Kilitleme / Açma Sesli İkaz (Keyless Buzzer) | `BCM 3B 2B` | Bip Sesi AÇIK (`01`) / KAPALI (`00`) |
| **HF-FOLLOWHOME** | Eve Kadar Takip (Follow Me Home) Far Süresi | `BCM 3B 39` | 0 sn / 10 sn / 15 sn / 20 sn / 25 sn / 30 sn |
| **HF-SUPERLOCK** | Süper Kilitleme / Deadlock Ayar & Testi | `BCM 30 02` | Süper Kilit Aktif / İptal |
| **HF-ESP-DYNO** | ESP Dyno / Off-Road Tam Devre Dışı Modu | `ABS 30 80 07` | ESP Tam Kapalı (`04`) / Normal ESP (`00`) |

---

### G. Gizli Özelliklerin Modele Göre Dinamik Filtrelenmesi ve Uyumsuz Özelliklerin Kilitlenmesi
- **Gereksinim:** Araç modelleri arasında donanım farkları bulunmaktadır (örneğin Grand Vitara JB'de mekanik flasör rölesi ve manuel aynalar bulunurken, modern CAN Vitara LY'de mikrodenetleyici kontrollü konfor sinyali ve elektrikli katlanır ayna BCM çıkışları vardır). Bir araçta donanımsal veya yazılımsal olarak bulunmayan gizli özelliklerin açık gösterilmemesi ve araca yanlış komut yazılmasının önlenmesi istendi.
- **Uygulanan Model Uyumluluk Matrisi:**
  - **Grand Vitara (1.9 DDiS / Benzinli - JB Serisi):** 11 adet fabrika fonksiyonu (`HF-ALARM`, `HF-ANSWERLIGHT`, `HF-AUTOLOCK`, `HF-AUTOUNLOCK`, `HF-UNLOCKMODE`, `HF-ROOMLAMP`, `HF-RETAINEDPWR`, `HF-AUTOLIGHT-HW`, `HF-BUZZER`, `HF-FOLLOWHOME`, `HF-ESP-DYNO`) tam destekli ve aktif.
  - **Uyumsuz / Olmayan 10 Özellik (Grand Vitara'da Kapalı):** `HF-DRL`, `HF-LANEFLASH`, `HF-MIRROR`, `HF-CORNER`, `HF-GUIDECAR`, `HF-FOOTLIGHT`, `HF-HBA`, `HF-UDS-IKEY`, `HF-SEATBELT`, `HF-CLUSTER-THEME`.
- **Çift Katmanlı Koruma & Kullanıcı Deneyimi:**
  1. **Arayüz (Frontend):** Uyumsuz satırlar soluklaştırılır (`opacity: 0.45`), başlığın altında kırmızı `❌ BU MODELDE DONANIM / BCM DESTEĞİ YOK (KAPALI)` ikazı çıkar. Seçenek açılır kutusu (`<select>`) ve buton (`<button>`) devre dışı bırakılır (`disabled`), butonda `❌ Bu Modelde Yok` yazar.
  2. **Model Filtreleme Onay Kutusu:** Tablonun üstündeki filtre işaretlendiğinde sadece o araca uyan özellikler gösterilir (Grand Vitara için 11 özellik).
  3. **C++ Sunucu Güvenlik Kilidi (`ExecuteHiddenFeatureCoding`):** Backend seviyesinde de aktif araç profili kontrol edilir. Uyumsuz bir kodlama talebi geldiğinde sunucu donanım güvenliği gereği işlemi reddederek `FEATURE_NOT_SUPPORTED` hatası döndürür.

### H. Tüm 5 Sekmede Uçtan Uca Modele Özel Çalışma Mimarisi (Kritik Güvenlik)
Kullanıcının seçtiği araç profiline (`Vehicle Profile`) veya `Auto Detect` sonucu tespit edilen ECU kimliğine göre 5 sekmenin tamamı modele uyum sağlar:
1. **Sekme 1 (Sistem Taraması & DTC):** `GetActiveSuzukiModules()` fonksiyonu Grand Vitara 1.9 DDiS için sadece JB mimarisindeki 9 modülü (F9Q CAN ECM, ABS/ESP 0x29 K-Line, 4WD 0x21, BCM 0x5A vb.) hedefler. Yeni Suzuki CAN için 13 UDS modülü taranır.
2. **Sekme 2 (Canlı Veri & Telemetri):** Benzinli (`gv_petrol`) ve klasik K-Line (`suzuki_kline`) profillerinde dizel parametreleri (DPF Kurum Miktarı, DPF Sıcaklığı, DPF Diferansiyel Basıncı) tablodan otomatik olarak gizlenir. Bosch F9Q blokları (`21 A4..C0`) sadece dizel Grand Vitara'da çalışır.
3. **Sekme 3 (Servis & Kalibrasyon):** 27 adet kalibrasyon kartı `filterServiceByVehicle` ile modele göre filtrelenir; F9Q Enjektör IMA, DPF ve EGR adaptasyonları sadece uyumlu araçta açılır. 631 Aktif Test ve 570 Servis Fonksiyonu kataloğu otomatik olarak Grand Vitara için **Base 93**, CAN için **Base 41**, K-Line için **Base 64** veritabanına bağlanır.
4. **Sekme 4 (ECU Kimlik & K-Line):** Suzuki 4-baytlık K-Line ISO 14230 protokolü üzerinden Pin 7 üniteleri doğrudan hedeflenir.
5. **Sekme 5 (Gizli Özellikler & BCM Kodlama):** Uyumsuz olan tüm özellikler devre dışı bırakılır (`disabled`), araç beynine yanlış komut yazılması hem frontend hem C++ backend katmanında engellenir.

---

### I. BCM Kodlamasının Sayfaya Canlı Düşmesi ve Görsel Durum Rozetleri
- **Özellik Okuma:** `Mevcut Araç Kodlamasını Oku (21 08 / 21 01)` butonuna tıklandığında, araç beyninden okunan ham EEPROM baytları (`states`) anında çözümlenir.
- **Canlı Görsel Gösterim:**
  - Açık / Aktif olan özellikler: Yeşil renkli `● AÇIK / AKTİF` rozeti ve araçtaki tam ayar açıklaması (`Araç Ayarı: AÇIK`).
  - Kapalı / Pasif olan özellikler: Gri renkli `○ KAPALI` rozeti (`Araç Ayarı: KAPALI`).
  - Açılır kutu (`<select>`) otomatik olarak aracın o anki değerini seçili hale getirir.
  - Modelde donanımı olmayan özellikler: Kırmızı renkli `❌ BU MODELDE DONANIM / BCM DESTEĞİ YOK (KAPALI)` rozeti ve kilitli (`disabled`) buton ile gösterilir.
- **K-Line BCM-06 Genişletilmiş Çözümleme:** Grand Vitara JB Pin 7 BCM'inden okunan 37 baytlık `21 08` paketindeki Alarm, Kilitleme Sinyal Onayı, Hıza Duyarlı Kilit, Kontak Kapatınca Açma, Tek Basış Açma Modu, Bip Sesi (Buzzer), Follow Me Home, Tavan Lambası Dimmer ve Cam Kaldırma süreleri eksiksiz olarak çözümlenmektedir.

---

### J. İmmobilizer / Keyless (KLS) Mimarisi ve K-Line Pin 7 Hedef 0x5D (Base 70 / Base 46) Düzeltmesi
1. **Canlı Araçta Karşılaşılan Hata (`7F 21 12` & Yanıtsızlık) Nedenleri:**
   - Sekme 3'te İmmobilizer durumu ve kayıtlı anahtar sayısı okunurken `21 A1` / `21 01` ve `21 B0` komutlarında:
     - CAN `0x7E0` (Bosch EDC16C39 ECM): `TX [21 B0] -> RX [7F 21 12]` (NRC 0x12: Sub-function not supported)
     - CAN `0x271` ve K-Line `0x5A` (BCM): Yanıt yok.
     - CAN `0x264` ve K-Line `0x5B` (İmmobilizer): Yanıt yok.
2. **Kök Neden & Mühendislik Analizi:**
   - **Transponder Anteni Motor Beynine Bağlı Değildir:** Grand Vitara JB419D (F9Q) modelinde kontak yuvasındaki transponder bobini motor beynine değil, direksiyon kolonunun sağındaki **Keyless Start & İmmobilizer Kontrol Modülü (33970-67J0)**'ne fiziksel olarak bağlıdır. Dizel Bosch EDC16C39 motor beynine `21 B0` (Transponder ID Oku) gönderildiğinde beynin `7F 21 12` (Bu alt fonksiyon desteklenmiyor) dönmesi mimari olarak tamamen doğrudur.
   - **KLS Modülünün Doğru K-Line Adresi 0x5D'dir:** Yazılım daha önce Benzinli Suzuki'lerin Base 41 / Base 36 (`0x5B`) şablonunu kullanıyordu. Oysa Grand Vitara JB serisinde Keyless & Immobilizer modülü **K-Line Pin 7, Hedef Adresi `0x5D`** (SDT-II Base 70 / Base 46; CAN tarafında ID `0x275`) üzerindedir!
3. **Uygulanan Çözüm:**
   - `suzuki_utilities.h` içerisindeki tüm immobilizer aksiyonları (`read_status`, `read_transponder`, `read_pin`, `verify_pin`, `register_key`, `register_remote`, `delete_all_keys`), Grand Vitara için **Base 70 (K-Line Pin 7 hedef `0x5D`)** ve **Base 46 (CAN `0x275`)** öncelikli olacak şekilde güncellendi.
   - `suzuki_diag_server.cpp` içindeki `/api/kline/imm` rotası hedef adres olarak varsayılan `0x5D` (otomatik `0x5B` fallback) olarak uyarlandı.
   - Böylece OBD araca tekrar takıldığında immobilizer sorguları doğrudan direksiyon altındaki gerçek `33970-67J0` beynine gidecek ve transponder ID ile kayıtlı anahtar bilgisi sorunsuz okunacaktır.

---

### K. Bosch EDC16C39 (F9Q) Şasi Numarası (VIN) Elektronik Hafıza Analizi
1. **Mühendislik Tespiti (SDT-II Kataloğu Base 93):**
   - SDT-II resmi fabrika veritabanı incelenmiştir (`compatibility\catalog\16330983.json`):
     ```json
     "supports_vin_registration": "false"
     ```
   - Suzuki Bosch EDC16C39 (Renault F9Q 1.9 DDiS) motor kontrol üniteleri fabrikasyon olarak **EEPROM veya Flash hafızasında elektronik Şasi Numarası (VIN) SAKLAMAZ**.
   - Orijinal Suzuki SDT-II yetkili servis cihazında da bu motora bağlanıldığında VIN hanesi boş / "No VIN Found" olarak gelir. ECM hafızasında Renault Parça No (`8200638060`), HW (`8200518648`), SW (`00C1`), Kalibrasyon No (`5000`) ve Enjektör IMA kodları (`21 AD`) saklanır.
2. **Kullanıcı Çözümü & Kalıcılık:**
   - Üst durum çubuğundaki (Topbar) **Şasi No (VIN)** rozetine tıklandığında kullanıcının araç şasi numarasını girmesine olanak tanıyan hızlı giriş kutusu eklendi.
   - Girilen şasi numarası tarayıcının `localStorage` alanına kalıcı olarak kaydedilir ve OBD takılı olmasa dahi sayfada sürekli olarak görüntülenir.

---

### L. İmmobilizer & Transponder Kodlamasının Modele Göre Dinamik Mimarisi (`SuzukiImmoSystem`)
- **Mühendislik Gereksinimi:** Suzuki modelleri arasında İmmobilizer ve Anahtar Programlama donanım mimarisi ve güvenlik protokolleri köklü farklılıklar gösterir:
  1. **Grand Vitara 1.9 DDiS (`gv_jb419d`):** Kontak transponder anteni motor beynine değil, direksiyon altındaki **Keyless Start & İmmobilizer Modülü (33970-67J0)**'ne bağlıdır. İletişim **K-Line Pin 7 Hedef `0x5D` (Base 70)** ve **CAN ID `0x275` (Base 46)** üzerindedir. Bosch EDC16C39 motor beyninde transponder saklanmaz. Güvenlik 4-Haneli PIN (`0895`) iledir. UDS Seed-Key bu modelde yoktur.
  2. **Grand Vitara Benzinli (`gv_petrol`):** Denso/Hitachi Motor Beyni (**CAN `0x7E0` / Base 41**) ile KLS Beyni (**CAN `0x275` / K-Line `0x5D`**) entegre çalışır. 4-Haneli PIN (`0895` / `9502`) kullanılır. UDS Seed-Key bu modelde yoktur.
  3. **Klasik Suzuki K-Line (`suzuki_kline`):** 2000–2008 Jimny SN413, Swift MZ KWP, Ignis, Alto, Wagon R modellerinde saf Pin 7 K-Line hattı kullanılır. İmmobilizer **Harici IMM Beyni (`0x5B` / Base 36)** veya **ECM KWP (`0x11` / Base 64)** üzerindedir. CAN hattı bulunmaz. 4-Haneli PIN (`0895` / `5634`) kullanılır.
  4. **Modern Suzuki CAN UDS (`suzuki_can`):** 2015+ Vitara LY, SX4 S-Cross JY, Swift AZ, Jimny JB74 modellerinde modern Gövde & Keyless Beyni (**CAN UDS `0x7E4` / `0x750` / Base 169**) bulunur. Bu araçlarda klasik 4-haneli K-Line PIN kodu kabul edilmez; **Servis 27 (27 03 Request Seed -> 27 04 Send Key)** ve **31 01 01 0D Anahtar Kayıt Rutini** kullanılır.
- **Arayüz (Sekme 3) Dinamik Adaptasyonu:**
  - İmmobilizer panelinin en üstüne şık bir **Model Mimari Bilgi Rozeti** (`#immoModelArchCard`) eklendi.
  - Seçilen araç profiline göre panel anında şekil alır:
    - Grand Vitara ve Klasik K-Line seçildiğinde: Bölüm 2 (4-Haneli PIN) tam aktif olur; Bölüm 3 (UDS Seed-Key) soluklaştırılarak butonları kilitlenir (`❌ BU MODELDE UDS YOK`).
    - Modern CAN UDS seçildiğinde: Bölüm 3 (UDS Seed-Key) yeşil rozetle tam aktif olur; Bölüm 2 (4-Haneli PIN) kilitlenir (`❌ BU MODELDE PIN YOK (UDS GEREKLİ)`).
- **C++ Backend Çift Katmanlı Güvenlik Kilidi:**
  - `ExecuteImmobilizerAction()` fonksiyonu `SuzukiImmoSystem` enum tipi ile güçlendirildi.
  - Transponder ID (`21 B0`) ve Anahtar Sayısı (`21 A1`) sorguları araca göre doğrudan doğru adrese (`gv_jb419d` için `0x5D`, petrol için `0x7E0`, klasik için `0x5B`, UDS için `0x7E4`) yönlendirilir.
  - UDS olmayan araca UDS komutu veya UDS araca eski K-Line PIN komutu gönderilmesi sunucu tarafında donanım güvenliği gereği reddedilir.

---

## 2. Araç Üzerinde Yapılan Canlı Testlerin Kapsamlı Durum Matrisi

Suzuki Grand Vitara 1.9 DDiS (JB419D) aracı üzerinde bugüne kadar yapılan canlı OBD/J2534 bağlantı testleri, donanım engelleri ve bir sonraki bağlantıda sırada bekleyen işlemler aşağıda kategorize edilmiştir:

### ✅ A. Canlı Araçta Denenen ve Doğrulanan Fonksiyonlar (Okuma / Tarama / Telemetri)

| # | Sistem / Modül | İletişim Hattı | İşlem / Servis | Sonuç ve Detay |
| :- | :--- | :--- | :--- | :--- |
| 1 | **Bosch EDC16C39 Motor Beyni** | CAN (ID `0x7E0`) | Özel Oturum Açma (`10 C0`) | **BAŞARILI.** Beyin `50 C0` pozitif yanıtı vererek oturumu açtı. |
| 2 | **Motor Canlı Sensör Telemetrisi** | CAN (ID `0x7E0`) | Standart + F9Q Blokları (`21 01`, `21 A4..C0`) | **BAŞARILI.** Devir (800 RPM), Rail Basıncı (298 bar), Akü (12.1V), Soğutma Suyu (84°C), DPF Sıcaklığı/Basıncı canlı okundu. |
| 3 | **Motor Arıza Kodu (DTC) Taraması** | CAN (ID `0x7E0`) | Arıza Okuma & Silme (`18 00 FF 00` / `14 FF 00`) | **BAŞARILI.** Aktif ve geçmiş motor arızaları listelendi. |
| 4 | **BCM Gövde Kontrol Modülü** | K-Line Pin 7 (`0x5A`) | Suzuki 4-Baytlık ISO 14230 Oturumu | **BAŞARILI.** `0x80 0x5A 0xF1 0x02 0x10 0xC0` ile BCM oturumu açıldı. |
| 5 | **BCM Fabrika Konfigürasyon Okuma** | K-Line Pin 7 (`0x5A`) | EEPROM Okuma (`21 08`) | **BAŞARILI.** 37 baytlık konfigürasyon paketi okundu; Alarm, Sinyal onayı, Kilit modları çözümlendi. |
| 6 | **BCM Gizli Özellik Durum Çözümleme** | Arayüz (Sekme 5) | Canlı EEPROM Deşifresi | **BAŞARILI.** Araçtaki mevcut ayarlar sayfaya düştü (Yeşil `● AÇIK`, Gri `○ KAPALI` rozetleri). |
| 7 | **ABS / ESP Fren Beyni Canlı Veri** | K-Line Pin 7 (`0x29`) | Telemetri Okuma (`21 00`) | **BAŞARILI.** 34 baytlık çerçeve okundu: Direksiyon Açısı (`7.0°`), Yanal İvme (`0.00 G`), Savrulma (`0.0 °/s`), Akü Voltajı (`12.10 V`). |
| 8 | **ABS / ESP Arıza Taraması (DTC)** | K-Line Pin 7 (`0x29`) | Arıza Kodu Okuma (`18 00 FF 00`) | **BAŞARILI.** `C1021: Ön Sağ Tekerlek Hız Sensörü (Sinyal Yok / Devre Açık)` hatası anında tespit edildi. |
| 9 | **Model Bazlı Sekme & Özellik Filtreleme** | Tüm Sekmeler (1-5) | Dinamik Profil Kısıtlama | **BAŞARILI.** Grand Vitara JB için 11 aktif özellik gösterildi, donanımda olmayan 10 özellik kilitlendi. |
| 10 | **Şasi No (VIN) Üst Rozet & Kalıcılık** | Üst Durum Çubuğu | Topbar Badge & `localStorage` | **BAŞARILI.** Tıklanabilir, kopyalanabilir ve tarayıcıda kalıcı olarak saklanabilir hale getirildi. |
| 11 | **Canlı Telemetri CSV Kaydı** | Arayüz (Sekme 2) | Dosyaya Loglama | **BAŞARILI.** Zaman damgalı CSV dosyası masaüstüne/indirilenlere kaydedildi. |

---

### ⚠️ B. Canlı Araçta Denenen Fakat Donanım / Tesisat Engeli Tespit Edilenler

| # | Fonksiyon | Komut / Modül | Karşılaşılan Durum | Tespit Edilen Donanımsal / Fiziksel Engel |
| :- | :--- | :--- | :--- | :--- |
| 1 | **Direksiyon Açı Sensörü (SAS) Kalibrasyonu** | ABS-03 / `A5 03` (K-Line `0x29`) | Araç `7F A5 23` (NRC 0x23: Şartlar Uygun Değil) ile reddetti. | **1. Engel:** `C1021: Ön Sağ Tekerlek Hız Sensörü` arızası varken Bosch ESP sistemi güvenlik protokolü gereği kalibrasyona izin vermez.<br>**2. Engel:** Direksiyon açısı tam düz (`0.0° ± 1.5°`) değil, `7.0°` açıda duruyor. |
| 2 | **Hız Sabitleyici (Cruise Control)** | ECM (Bosch EDC16C39) | Kadranda devreye girmiyor. | **1. Engel:** Aktif ESP lambası ve `C1021` arızası varken motor beyni hız sabitlemeyi kilitler.<br>**2. Engel:** Direksiyon zembereği (clock spring) kopukluğu veya direksiyon simidi tuş takımının iletim sorunu. |
| 3 | **Elektronik Şasi No (VIN) Okuma** | ECM (`1A 90` / `22 F1 90` / `09 02`) | ECM yanıt vermedi ("Could not read VIN"). | **Mimari Gerçek:** Grand Vitara 1.9 DDiS (Bosch EDC16C39 - Base 93) motor beyni fabrikasyon olarak EEPROM/Flash hafızasında elektronik VIN **saklamaz** (`supports_vin_registration: false`). SDT-II yetkili cihazında da bu motorda VIN okunmaz. |
| 4 | **Motor Beyninden Transponder ID Okuma** | ECM CAN `0x7E0` (`21 B0`) | Beyin `7F 21 12` (NRC 0x12: Sub-function not supported) döndü. | **Tesisat Gerçeği:** Kontak transponder anten bobini motor beynine değil, direksiyon altındaki **Keyless Start & Immobilizer Kontrol Modülü (33970-67J0)**'ne bağlıdır. Bu fonksiyon ECM'den değil, K-Line `0x5D` üzerinden okunmalıdır. |

---

### ⏳ C. Kodlanan / Düzeltilen ve Bir Sonraki OBD Bağlantısında Denenecekler

Aşağıdaki fonksiyonlar offline olarak kodlanmış, derlenmiş ve test edilmek üzere hazır bekletilmektedir:

1. **Keyless & İmmobilizer Modülü (K-Line Pin 7, Hedef `0x5D` - Base 70 / Base 46):**
   - Transponder Anahtar ID Okuma (`21 B0` -> `0x5D`).
   - Kayıtlı Anahtar Sayısı & IMMO Durumu (`21 A1` / `21 01` -> `0x5D`).
   - PIN Kodu Doğrulama & Kumanda Tanıma (`IMMO-02`, `IMMO-03`).
2. **BCM Gizli Özellik Değiştirme / Yazma (K-Line `0x5A` - `3B` Servisi):**
   - BCM EEPROM okuması doğrulandı; şimdi örneğin **Hıza Duyarlı Otomatik Kilitleme (15 km/s)** veya **Eve Kadar Takip (Follow Me Home Far Süresi)** gibi bir ayarı değiştirip araca yazma testi.
3. **Enjektör IMA Kalibrasyon Kodları Okuma & Yazma (DSL-01 / SendBatch):**
   - Kesintisiz tek J2534 oturumunda (`10 C0` -> `21 AD`) 4 silindirin 7 haneli enjektör kodlarının çekilmesi ve gerekirse yeni kod yazılması (`3B AD FF`).
4. **C1021 ve Direksiyon Düzeltildikten Sonra SAS Kalibrasyonu (ABS-03):**
   - Ön sağ ABS sensör kablosu/soketi onarılıp direksiyon tam düz (`0.0°`) konuma alındıktan sonra Sekme 3'ten SAS kalibrasyonu butonuna basılarak ESP lambasının söndürülmesi testi.

---

### 🔬 D. İleri Seviye / İsteğe Bağlı Henüz Denenmemiş Servis ve Aktüatör Fonksiyonları

Gerektiğinde araç üzerinde denenebilecek diğer gelişmiş servis fonksiyonları:

- **DPF Servis Rejenerasyonu Başlatma (DSL-02 / `31 01`):** Araç durduğu yerde sabit devirde partikül filtresi kurum yakma işlemi.
- **DPF Diferansiyel Basınç Sensörü & Kül Sıfırlama (DSL-03 / `3B 7E FD`):** DPF değişim veya temizlik sonrası adaptasyon.
- **EGR Valfi Pozisyon Adaptasyonu (DSL-04 / `3B 7E FC`):** EGR temel ayar sıfırlama.
- **Aktif Aktüatör Çalıştırma Testleri (Sekme 3 Kataloğu):**
  - Yakıt Pompası Rölesi Tetikleme
  - Radyatör Soğutma Fanı Kademe 1 & Kademe 2 Testi
  - EGR Selenoid Valf Açma/Kapama Testi
  - Silindir Sıkıştırma / Enjektör İptal Dengeleme Testi
  - ABS Hidrolik Fren Valf Kanatma (Brake Bleeding) Testi
- **4WD / Diferansiyel Kilit Modülü Taraması (K-Line `0x21`):**
  - Transfer kutusu 4H / 4L kilit konum sensörleri ve aktüatör durum kontrolü.
- **SRS Hava Yastığı Modülü Taraması (K-Line `0x59`):**
  - Kaza/çarpışma kayıtları ve piroteknik kemer/yastık direnç değerlerinin okunması.

---

## 3. Resmî Suzuki SDT-II Veritabanı Karşılaştırması & Eksikler / Gelecek Geliştirmeler Analizi

Orijinal Suzuki SDT-II servis yazılımının şifresi çözülmüş SQLite veritabanı (`compatibility\db-analysis\ngds2.decrypted.db` - 67 tablo) ve katalog dosyaları (`compatibility\catalog\`) ile mevcut yazılımımız karşılaştırıldığında durum şöyledir:

### 📊 Veritabanı Varlık Karşılaştırması

| Veritabanı Varlığı | Resmî SDT-II Veritabanı | Suzuki OBD Professional (Mevcut Durum) | Durum & Değerlendirme |
| :--- | :--- | :--- | :--- |
| **Arıza Kodları (DTC)** | **18,329** Adet Fabrika Kodu | **18,329** Adet Açıklamalı DTC | **Veritabanı Eşleşti.** Motor, Şanzıman, Fren, Gövde, Ağ ve Hava Yastığı kod tanımları aktarılmıştır. |
| **ECU Varyantları & Base ID** | **3,806** Varyant / **238** Base ID | **3,806** ECU Varyant / **238** Base ID | **Veritabanı Eşleşti.** SDT-II kontrol ünitesi kimlik tabloları `suzuki_db_catalog.h` içine aktarılmıştır. |
| **ECU Kimlik İmzaları (Ident)** | **4,421** Bayt İmzası | **4,421** İdent Kuralı | **Veritabanı Eşleşti.** Otomatik araç ve modül tanıma (`Auto Detect`) bu imzaları kullanır. |
| **Aktif Aktüatör Testleri** | **1,426** Adet Resmî Test | **631** Katalog Kaydı & Ham Komut Adayı | Çoğu test katalog verisi ve temel ham hex komut adayı olarak sunulmuştur; interaktif çok adımlı sihirbazların tamamı ve canlı araç doğrulaması henüz mevcut değildir. |
| **OEM Servis Fonksiyonları (Utility)** | **988** Adet Resmî Prosedür | **570** Katalog / Komut Adayı + **27** Özel Hızlı Kart | Veritabanındaki parametrik/interaktif servislerin önemli bölümü katalog verisi veya basit komut adayıdır; 27 özel kartta kalıcı oturum ve adım doğrulama kilitleri bulunur, araç üzeri yazma testleri beklemektedir. |
| **Canlı Veri Parametreleri** | **25,016** PID / 221,454 Bağlantı | Çekirdek Motor, Turbo, Yakıt, DPF, SAS, Savrulma, Akü Telemetrisi | Grand Vitara 1.9 DDiS üzerinde ana telemetri canlı doğrulanmıştır; diğer modeller DB uyarlamasıdır. |
| **İmmobilizer & Anahtar Mimarisi** | Model bazlı KLS, IMM ve UDS | `SuzukiImmoSystem` + İki Aşamalı Oturum Doğrulama | Model/protokol yönlendirmesi ve fail-closed kilit eklenmiştir; canlı araçta anahtar kaydetme/silme henüz denenmemiştir. |
| **Servis & Kalibrasyon (Sekme 3)** | Model bazlı filtreleme ve donanım kilidi | `ResolveEffectiveProfile` + `IsServiceSupportedForProfile` | Tanımlanmamış `auto` veya bilinmeyen profilde yazma/kodlama işlemleri engellenir. |

---

### M. Model Bazlı Servis & Kalibrasyon Güvenlik Mimarisi (Services & Calibrations)

Sekme 3 ("Servis & Kalibrasyon") altında yer alan tüm hızlı servis kartları, Enjektör IMA kodlama paneli ve OEM prosedürleri seçili araç profiline göre dinamik ve korumalı hale getirilmiştir:

1. **Çapraz Model Hata Koruması:**
   - Grand Vitara Benzinli (JB416/JB420/JB424) seçildiğinde dizel motorlara özgü DPF Rejenerasyonu, DPF Kül Sıfırlama, F9Q Enjektör IMA Kodlama, F9Q EGR Adaptasyonu ve F9Q Boğaz Kelebeği prosedürleri otomatik olarak kilitlenir (`❌ Bu Modelde Yok`).
   - Grand Vitara 1.9 DDiS seçildiğinde benzinli gaz kelebeği öğrenme, elektrikli direksiyon (EPS - araçta hidrolik direksiyon vardır), elektronik el freni (EPB) ve hibrit (SHVS) fonksiyonları devreden çıkarılır.
   - Klasik K-Line modellerde CAN'a özgü UDS servisleri gizlenir ve butonlar pasif hale getirilir.
2. **Çift Kademeli Doğrulama (Frontend & Backend Güvenliği):**
   - **Frontend:** `renderServicesForVehicle()` fonksiyonu seçili profile göre uyumsuz satırları saydamlaştırır (`opacity: 0.45`), butonları kilitler (`disabled`, cursor `not-allowed`) ve filtre açıkken gizler. Kullanıcı butona doğrudan tıklasa dahi `runService()` model uyuşmazlığını tespit edip işlemi anında durdurur. Benzinli seçildiğinde `imaToolCard` paneli de otomatik olarak kapatılır.
   - **C++ Backend:** `suzuki_utilities.h` içerisinde `IsServiceSupportedForProfile()` fonksiyonu tanımlanmış; `/api/calibrate/<subpath>` ve `/api/injector/ima` uç noktalarında model kontrolü eklenmiştir. Uyumsuz bir modelden istek gelirse ECU güvenliği için işlem reddedilir (`SERVICE_NOT_SUPPORTED`).

---

---

### 🔍 Doğrulama Kapsamı, Eksikler & Gelecek Geliştirme Yol Haritası (Roadmap)

**Doğrulama Gerçeği:**
- Canlı araçta (Grand Vitara 1.9 DDiS JB419D) **oturum açma, DTC okuma/silme, motor/ABS canlı veri akışı ve BCM `21 08` okuma** fiziksel olarak doğrulanmıştır.
- Buna karşın **anahtar kaydetme/silme, BCM yazma, enjektör IMA yazma, DPF/EGR/Throttle sıfırlama ve farklı Suzuki şasileri** SDT-II veritabanından uyarlanmış prosedürlerdir ve **canlı araçta henüz sınanmamıştır**. Derleme sırasında çalıştırılan `--check-identity` testi donanım açmaz ve yalnızca veritabanı kimlik/tablo asgari kayıt kontrollerini doğrular; kalibrasyon, kodlama ve J2534 yazma oturumları araç üzerinde test edilmelidir.

Yazılımı daha da ileriye taşımak için SDT-II veritabanından ileride eklenebilecek opsiyonel özellikler şunlardır:

1. **CAN Bus Ağ Geçidi Topoloji Kontrolü (`Bus Check`):**
   - SDT-II veritabanındaki `bus_check_vehicle` (199 araç modeli) ve `bus_check_vehicle_dtc_mapping` (7,938 kural) tabloları kullanılarak aracın tüm CAN High, CAN Low ve LIN hatlarındaki modül bağlantı kopukluklarının (`U0100`, `U0121`, `U0140` vb.) tek ekranda grafik ağaç şeması olarak taranması.
2. **Kalan 418 Adet Özel Servis Fonksiyonunun Canlı Kataloğa Eklenmesi:**
   - 988 utility'nin 570 tanesi hazır durumdadır. Kalan nadir prosedürler (örneğin bazı pazarlara özel CNG kalibrasyonları, hibrit akü dengeleme testleri) ihtiyaç halinde katalog JSON'larından dinamik olarak çağrılabilir.
3. **Otomatik Şasi Numarasından (VIN) Model Varyantı Çözümleme:**
   - Girilen 17 haneli VIN numarasının ilk 7-8 hanesinden (`JSAJTD54V...` -> JB420, `JSAJTD44V...` -> JB419D F9Q) otomatik araç profili seçimi.
4. **2018+ Modeller için Güvenlik Ağ Geçidi (SGW - Security Gateway) Kilidi:**
   - Yeni nesil 2018 sonrası Suzuki modellerinde (Swift Sport ZC33S, Jimny JB74 2021+) yer alan korumalı CAN Gateway kilidi için çevrimdışı token desteği.

---

## 4. 11 Kasım 2026 Teknik Denetim ve Güvenlik Sertleştirme Düzeltmeleri

Bağımsız teknik inceleme sonucunda tespit edilen ilk 6 kritik hata, ikinci turda bildirilen 4 ek hata ve üçüncü turda bildirilen 6 ek bulgu giderilmiştir:
1. **Çok Adımlı Prosedür Doğrulaması (`SequenceMode`, `ExecuteStagedSequenceOnBase`):** Oturum (`10 C0` / `10 03`) reddedildiğinde sonraki komutların gönderilmesi durduruldu. Enjektör IMA yazma (`3B AD FF` + `3B 7E FE`), VIN yazma (`3B 90` / `2E F1 90`), İmmobilizer anahtar kaydetme/silme (`Stage 1 PIN -> Stage 2 İşlem -> Stage 3 21 A1`), DPF rejenerasyon (`3B AC`) ve yüksek basınç testinde (`31 62` / `21 62`) önceki adım başarısızken son okumanın işlemi başarılı göstermesi engellendi.
2. **Kalıcı CAN & K-Line Oturum Kapsamları (`CanChannelScope`, `KLineChannelScope`):** `ExecuteSASCalibration`, `ExecuteDPFReset`, `ExecuteOilReset`, `ExecuteEGRInit`, `ExecuteThrottleInit`, `ExecuteInjectorZfcInit`, `ExecuteDPFRegen`, `ExecuteHighPressureTest`, `ExecuteClearAllDTCs`, `ExecuteHexSequenceOnBase` ve `ReadBcmCodingStateJson` fonksiyonları her karede `ptDisconnect()` yapmak yerine işlem boyunca tek kalıcı oturum kanalı kullanacak şekilde güncellendi.
3. **CAN ve K-Line Varyant Komut Ayrımı:** `ExecuteHiddenFeatureCoding` içinde CAN (`30 xx`) ve Klasik K-Line (`3B xx`) komutları profil/protokol bazlı ayrıldı (`tryCanThenClassic`); aynı diziye karıştırılıp birinin hatasını diğerinin gizlemesi önlendi. Çoklu varyant aramalarında `SequenceMode::TryFirstPositive` ile ilk olumlu yanıtta durulur.
4. **Nötr SAS Durum Bildirimi:** `21 00` bayt 26 bit 6 (`0x40`) sıfır olduğunda tek başına zemberek/besleme arızası veya `C1037` varsayımı kaldırıldı (`UNSET_OR_UNCALIBRATED` / `DURUM BİTİ PASİF / KALİBRE DEĞİL`); `C1037` ve tesisat uyarısı yalnızca aktif `C1037` DTC'si varsa gösterilir.
5. **Model & Gerçek ECU Varyant Kilidi (`UNIDENTIFIED` Tam Yazma Kilidi):** `ResolveEffectiveProfile()`, `ResolveDbTargetBase()` ve `IsServiceSupportedForProfile()` ile tanımlanmamış `auto` modunda veya bilinmeyen profillerde BCM kodlama, İmmobilizer yazma/silme/PIN, IMA yazma, VIN yazma, **`sas` kalibrasyonu ve `clear` DTC silme dahil tüm yazma işlemleri** engellendi (fail-closed) ve veritabanından ilk Base ID'ye kör seçim kapatıldı.
6. **Doğrulanmamış Voltaj Bölme Düzeltmesinin Kaldırılması:** `ReadBatteryVoltage()` içindeki `24–45 V` aralığını `3`'e bölen doğrulanmamış düzeltme kaldırıldı; J2534 `READ_VBATT` ham mV değeri doğrudan `vbattMv / 1000.0` olarak döndürülür.
7. **K-Line Yanlış "Arıza Yok" Sonucunun Düzeltilmesi (`EvaluateKLineDtcScan`):** `1A xx` veya `21 08` gibi sorgulara cevap veren ancak `18 00 40 00` / `18 00 FF 00` DTC okumasını reddeden veya boş dönen K-Line modüllerinde `dtcCount = 0` atanması kaldırıldı (`dtcCount = -1` ve açık ret/hata mesajı).
8. **SAS Kalibrasyonu Akış & Sonrası Doğrulaması (`VerifySasPostCalibration`):** K-Line `0x29` TypeB/C (`A5 01`) başarılı olduğunda veya `7F A5 23` döndüğünde TypeA (`30 01 08`) komutunun gereksiz gönderilmesi önlendi (`KLineChannelScope`). Ayrıca CAN `ABS-03` (`70 01 08`) ve K-Line `0x29` başlangıç onayından sonra hem SAS durum biti (`21 00` `0x40` / `22 11 01`) hem de DTC listesi (`18 00 FF 00` / `19 02 FF`) pozitif okunup doğrulanmadan işlem başarılı sayılmaz (yalnızca DTC yokluğu yeterli kabul edilmez).
9. **İmmobilizer Yanıt Doğrulaması & Model Korumaları (`IsValidImmoKeyStatusResp`, `IsValidTransponderIdResp`, `ExecuteImmobilizerAction`):** `read_status` işleminde `21 80`, `21 A0` veya `21 01` pozitif yanıtlarının `61 A1` anahtar sayısı yerine geçmesi engellendi; `read_transponder` işleminde `21 A0` çıkarıldı ve geçerli `61 B0` / `62 01 B0` transponder verisi doğrulanır. `MODERN_CAN_UDS` modunda `register_remote`, `ecu_immo_init` ve `steering_unlock` komutlarının klasik PIN / Base 39 yoluna girmesi, `CLASSIC_KLINE` modunda ise `antenna_test` komutunun modern CAN UDS Base 169'a düşmesi engellendi.
10. **Aşama 3 Son Doğrulama & Enjektör IMA / VIN Doğrulamaları (`ExecuteStagedSequenceOnBase`, `ParseInjectorIma21AdResponse`, `ExecuteInjectorImaJson`, `ExecuteVinActionJson`):** `ExecuteStagedSequenceOnBase` içinde Aşama 3 doğrulama komutu (`register_key`, `delete_all_keys` için `21 A1 -> 61 A1`; VIN yazma için `1A 90` / `22 F1 90`) başarısız olursa veya geri okunan 17 haneli VIN yazılanla eşleşmezse işlem başarısız işaretlenir. Enjektör IMA okumada kısa/eksik `61 AD` yanıtları kabul edilmez; yazmada yalnızca 6–7 haneli kodlara izin verilir (8+ haneli kod sessizce kısaltılmaz) ve yazma sonrası `21 AD` geri okuması tüm silindirlerde birebir karşılaştırılır.
11. **Base 169 ve Nissan/UDS Modül Adres & Oturum Eşlemesi (`GetDbBaseComm`, `IsUdsBase`, `HF-UDS-IKEY`, `RunAdasCalibProcedure`):** SDT-II veritabanında `NISSAN UDS-CAN BCM` (`Landy C27`) olarak yer alan Base `169`, varsayılan ECM `0x7E0/0x7E8` yerine gerçek adresi olan `0x745/0x765` CAN kimliklerine ve diğer `NISSAN UDS-CAN` / `TP` üslerine (`162..178`, `131..135` vb.) eşlendi; tanınmayan üsler sessizce `0x7E0`'a düşmek yerine fail-closed reddedilir (`UNKNOWN_BASE_ADDRESS`). `IsUdsBase(169)` `true` yapıldı (`10 C0` yerine `10 03` UDS oturumu). Base `169` (`HF-UDS-IKEY` ve `IMMO-110` Seed-Key) yalnızca Landy platformu doğrulandığında izin verilir; ADAS kalibrasyonu ise hiçbir durumda BCM Base `169`'a yönlenmez (Suzuki `191`/`186`/`143` veya eşleşmiş Nissan ADAS `163`/`164`/`132` kullanılır).
12. **Otomatik DB Base Seçiminde Platform Uyumluluğu Kilidi (`ResolveDbTargetBase`, `IsBaseCompatibleWithEffectiveProfile`):** Genel `suzuki_can` tercih listesinden Nissan Base `169` çıkarıldı ve eşleşmiş ECU / etkin profil platformuna yabancı Base ID'lerin (`NISSAN`/`TP4`/`TP5`) otomatik seçilmesi engellendi.
13. **UDS Seed Yanıtı Doğrulaması (`IsValidUdsSeedResp`):** Yalnızca pozitif `67` başlığına bakmak yerine alt fonksiyon (`01` / `03`), en az 4 baytlık seed uzunluğu (`>= 6` toplam bayt) ve tamamı `00` veya tamamı `FF` olmayan geçerli seed içeriği zorunlu kılındı; doğrulanan seed baytları mesajda raporlanır.
14. **Anahtar Kaydetme & Silmede Önce/Sonra Sayı Karşılaştırması (`VerifyImmoKeyCountTransition`, `ExecuteStagedSequenceOnBase`):** `register_key` ve `delete_all_keys` işlemleri aynı kalıcı oturumda işlem öncesi `21 A1` anahtar sayısını okuyup Aşama 3 sonrası sayıyla karşılaştırır (`register_key`: `postCount > preCount`; `delete_all_keys`: `postCount == 0`).
15. **Katı 4 Haneli PIN Doğrulaması (`ParseStrictImmoPin4`, `index.html`):** Boş PIN girildiğinde sessizce `0895` varsayılanına dönülmesi veya `4` haneden uzun PIN'in son `4` hanesinin kesilmesi hem C++ arka ucunda hem web arayüzünde kaldırıldı; eksik, fazla veya hex dışı PIN açık hata (`INVALID_PIN`) ile reddedilir.
16. **Veritabanı Kapsamının Açıkça Belirtilmesi:** Veritabanındaki `570` utility ve `631` aktif testin parametrik/interaktif bölümlerinin tam sihirbazlar değil katalog metadata'sı / temel komut adayları olduğu açıkça belgelendi.

> **Önemli Doğrulama Notu:** Bu sertleştirmeler MSVC x86 (`/W4`) derlemesi ve `--check-identity` donanımsız birim kontrolleri ile doğrulanmıştır; **ancak canlı J2534 cihazı ve gerçek araç üzerinde yazma/kalibrasyon/immobilizer/salt-okunur doğrulamalar (özellikle Grand Vitara 1.9 DDiS dışı şasiler, Base 169 ve tüm yazma komutları) henüz fiziksel olarak test edilmemiştir.**

---

## 5. Güncel Dosya ve Derleme Durumu

- **C++ Sunucu:** `suzuki_diag_server.cpp` + `suzuki_utilities.h` (MSVC x86 derlemesi tamamlandı, `suzuki_diag_server.exe --check-identity` geçti; donanım açılmadı).
- **Web Arayüzü:** `index.html` (Temiz, okunabilir, çift dilli, 5 sekmeli arayüz).
- **Senkronizasyon:** `c:\Users\kaany\Downloads\Suzuki SDT-II v2.28.01.30\dist` ve `D:\Claude\suzuki` klasörlerine kopyalanıp eşitlendi.
- **Sunucu Adresi:** `http://127.0.0.1:8890` (Çakışmaları önlemek için özel port `8890` olarak ayarlandı; meşgul olması durumunda otomatik olarak `8891..8899` aralığına geçer).
