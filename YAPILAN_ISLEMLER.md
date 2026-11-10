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
     - Direksiyon Açısı: `0.0°`
     - **Sensör Durum Baytı (Offset 26):** `0x00` (Bit 6 = 0: **Sinyal Ölü / Besleme Yok / Geçersiz**).
2. **Açının 0° Okunması ve Kalibrasyonun Reddedilmesi (NRC 0x23) Nedeni:**
   - Yapılan tam arıza taramasında ABS/ESP beyninde aktif 4 kritik arıza tespit edildi:
     - `C1037`: Direksiyon Açı Sensörü Güç Besleme Devresi Arızalı (Sensörde elektrik yok)
     - `U1126`: Direksiyon Açı Sensörü ile İletişim Kesik (CAN/LIN veri hattı kopuk)
     - `C1024`: Açı Sensörü Devre Arızası
     - `C1075`: Direksiyon Açı Sensörü Kalibrasyonu Eksik
   - ABS beynine K-Line Type B/C kalibrasyon komutu (`A5 01 80 00 00 00`) gönderildiğinde, beyin **`7F A5 23` (NRC 0x23: Koşullar Uygun Değil / Routine Rejected)** ile işlemi reddetmektedir.
   - **Teşhis:** Direksiyon açı sensörü elektrik beslemesi almadığı (kopuk döner yay / direksiyon zembereği veya sigorta) için ABS beynine açı iletemez ve ABS beyni beslemesiz/iletişimsiz sensörü kalibre etmeyi güvenlik gereği reddeder.
3. **Arayüz İyileştirmeleri:**
   - **Tab 1 (DTC):** `C1075` kodu tespit edildiğinde doğrudan satır içine **`[🎯 SAS Kalibrasyonunu Başlat (ABS-03)]`** aksiyon butonu eklendi.
   - **Tab 2 (Canlı Veri):** Canlı veri tablosuna `Direksiyon Açısı (SAS)` (`°`), `Savrulma Hızı` (`°/s`), `Yanal İvme` (`G`) ve yeni **`Direksiyon Sensörü Durumu`** parametresi eklendi. Sensörde besleme yokken kırmızı renkte `<span class="tag-fault">SİNYAL YOK / BESLEME ARIZASI (C1037)</span>` ikazı basılır.
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

## 2. Güncel Dosya ve Derleme Durumu

- **C++ Sunucu:** `suzuki_diag_server.cpp` (MSVC x86 derlemesi, 0 hata, 0 uyarı).
- **Web Arayüzü:** `index.html` (Temiz, okunabilir, çit dilli, 5 sekmeli, optimize edilmiş arayüz).
- **Senkronizasyon:** `c:\Users\kaany\Downloads\Suzuki SDT-II v2.28.01.30\dist` ve `D:\Claude\suzuki` klasörlerine anlık kopyalanıp eşitlendi.
- **Sunucu Adresi:** `http://127.0.0.1:8080` (Arka planda çalışır durumda).
