# Suzuki OBD Professional / J2534 Diagnostic Suite (BETA) — Güncel Durum ve Yapılan İşlemler

Son güncelleme: 9 Kasım 2026

> [!IMPORTANT]
> **BETA Sürüm & Donanım Doğrulama Bildirimi:**
> Bu yazılım şu an **BETA** aşamasındadır. Yalnızca **Suzuki Grand Vitara 1.9 DDiS (JB419D / Bosch EDC16C39 / Renault F9Q)** aracı ve J2534 (Tactrix OpenPort 2.0 / Scanmatik 2 Pro) donanımı üzerinde fiziksel olarak test edilmiş ve doğrulanmıştır. Diğer Suzuki modelleri, motor aileleri ve protokol kombinasyonları deneyseldir (kullanıcı sorumluluğundadır).

---

## 1. Tamamlanan Yazılım Güncellemeleri

1. **Varsayılan Dil ve Çift Dil (EN / TR) Desteği:**
   - Web arayüzü (`index.html`) ve sunucu konsolu varsayılan olarak **İngilizce (`en`)** açılır.
   - Sağ üst kısımdaki seçici ile istenildiği an **Türkçe (`tr`)** diline geçiş yapılabilir; tercih `localStorage` üzerinde saklanır.
   - Tüm sekme isimleri, arıza kodu başlıkları, canlı veri etiketleri ve servis prosedürleri çift dilde eksiksiz sunulur.

2. **Otomatik Araç / Model Algılama ve Modele Göre Dinamik Arayüz:**
   - `suzuki_diag_server.cpp` içindeki `AutoDetectSuzukiVehicle()` fonksiyonu 500 kbps CAN (`0x7E0`) üzerinden `1A 8E`, `1A 8A`, `22 F1 01`, `22 F1 88` ve `21 80` (Bosch EDC16C39 / F9Q) kimlik sorgularını; CAN yanıtı yoksa K-Line (`0x11`, `0x7A`, `0x10`) üzerinden `1A 8E`, `1A 8A`, `1A 94` ve `21 80` sorgularını otomatik çalıştırır.
   - Araç otomatik algılandığı (veya profil seçildiği) anda `index.html` üzerindeki 5 sekme modele göre dinamik güncellenir:
     - **1. Sekme (System Scan & DTC / Sistem Taraması):** Modele ait ECU listesi yüklenir ve `#dtcVariant` alanı otomatik olarak aracın `variantId` değeriyle doldurulur.
     - **2. Sekme (Live Data / Canlı Veri):** Benzinli / K-Line araçlarda DPF satırları gizlenir; algılanan `variantId` için Genişletilmiş Parametreler (Service 21 / 22) dinamik yüklenir.
     - **3. Sekme (Service & Calibration / Servis & Kalibrasyon):** `renderServicesForVehicle()` fonksiyonu üstteki kalibrasyon tablosunu algılanan modele uygun fonksiyonlara filtreler ve alttaki **Suzuki Aktif Testler (631) & Servis Fonksiyonları Kataloğu (570)** panelindeki `#dbCatalogBase` seçicisini algılanan `baseId` değerine (ör. F9Q için `Base 93`, CAN ECM için `Base 41`, UDS ECM için `Base 194`, K-Line ECM için `Base 64`) geçirerek o beyne ait aktif test ve servis fonksiyonlarını otomatik açar.
     - **4. Sekme (ECU Identification & K-Line / ECU Kimlik):** Okunan `CID`, `Parça Numarası`, `Varyant Adayları` ve aracın `Keşif Planı (Discovery Plan)` otomatik görüntülenir.
     - **5. Sekme (Hidden Features & Coding / Gizli Özellikler & Kodlama):** Seçilen veya otomatik algılanan araç profiline (`gv_jb419d`, `gv_petrol`, `suzuki_can`, `suzuki_kline`) göre **21 farklı gizli özellik ve BCM kişiselleştirme** seçeneği otomatik filtrelenir; aracın mevcut BCM ayar durumu (`21 08` / `21 01` CAN `0x7E4` ve K-Line `0x5A` Pin 7 üzerinden) okunup (`POST /api/coding/read`) yeni ayar doğrudan BCM EEPROM'una (`POST /api/coding/write`) kodlanabilir.

3. **62 Suzuki ECU Tabanı (`Base 33..200`) ve Otomatik CAN -> K-Line Yedekleme:**
   - CAN (`0x7E0..0x7EE`, `0x261..0x277`, `0x710..0x79F`) ve K-Line (`0x11`, `0x7A`, `0x10`, `0x19`, `0x21`, `0x29`, `0x31`, `0x59`, `0x5A`, `0x5B`, `0x5C`, `0x5D`, `0x98`, `0x99`, `0x01`, `0x09`, `0x0A`, `0x26`, `0x27`, `0x33`, `0xC0`, `0xD1`) adresleri eksiksiz tanımlandı.
   - CAN üzerinden yanıt alınamadığında hibrit / klasik Suzuki modellerinde sistem otomatik olarak K-Line (Pin 7) üzerinden yedek hatta geçer.

4. **23 Özel OEM Servis & Kalibrasyon Prosedürü:**
   - **F9Q 1.9 DDiS (`base 93` - 0x7E0):** Enjektör Kodlama (IMA `21 AD`), ZFC Sıfırlama (`3B 7E FE`), EGR Adaptasyon (`21 64 -> 3B 64`), Boğaz Kelebeği Başlatma (`2E 2F 1D`), DPF Kurum Sıfırlama (`31 0A 00`), ECM Değişimi Hafıza Transferi, DPF Servis Rejenerasyonu (`31 0D 00`), Yağ Değişim Işığı & Seyreltim Sayacı Sıfırlama (`21 6A -> 3B 6A`), Common Rail Sızdırmazlık Testi (`3B 62 -> 31 62 00`), Marş Motoru Montaj Kontrolü (`31 61 00`).
   - **CAN Dizel (`base 41` - D13A / D16AA / D20AA / K9K / J20 - 0x7E0):** CAN DPF Servis Rejenerasyonu (`30 72 08`), Yağ Değişim / Seyreltim Sıfırlama (`30 74 08 02`), EGR Öğrenme Sıfırlama (`30 70 08`), Sensör Değişim Sıfırlamaları (Fark Basınç, DPF, Volan, MAF, Rail).
   - **Benzinli & UDS Motor (`base 41, 194, 64` - M16A / J20A / J24B / K12 / K14C / H27A):** Gaz Kelebeği Kapalı Konum (`30 04 00` / `31 04 00`), Rölanti Hava Hacmi IAC (`31 03 00`), Yakıt Trim Öğrenme (`31 06 00`), Fren Strok Sensörü Sıfır Noktası (`30 D3 08 80` / UDS `31 01 C0 43 00`) ve GPF Başlatma.
   - **Şanzıman (`base 49, 75, 147` - AT / AMT / CVT - 0x7E1 / K-Line 0x19):** Otomatik Şanzıman Nötr Konum (`30 60 08 80`), AGS/AMT Debriyaj & Vites Öğrenme (`30 77/76/7A`), CVT Öğrenme & Eğim Sensörü (`30 00/20/51`, UDS `31 01 C1 08 00`).
   - **Şasi & Gövde (`base 34, 42, 39, 47, 38, 92` & K-Line):** ABS / ESP Direksiyon Açısı (SAS) & G-Sensör Kalibrasyonu (`30 01 08 80 80` / `30 16 00 00` / K-Line `A5 01 80 00`), ABS Hidrolik Fren Hava Alma (`30 00 07 01 01 00 00`), EPS Tork / Açı Sıfır Noktası (`30 11 08`), BCM Kaza Kaydı Silme (`30 07 08 08` / K-Line `A5 07 08`), TPMS ID Kayıt Modu (`30 08 07 80`), IPC Bakım İkaz Sıfırlama (`30 35 08`), ALC Far Sensör Başlatma (`30 00 08`), Tüm Modüllerde Toplu Arıza Silme.

5. **570 Servis Fonksiyonu ve 631 Aktüatör Testi Motoru:**
   - 570 servis fonksiyonu ve 631 aktüatör testi TR/EN başlıkları, desteklenen modül tabanları ve ham KWP2000/UDS komutlarıyla kataloglandı.
   - `POST /api/db-catalog`, `POST /api/active-test/run` ve `POST /api/utility/run` uç noktaları üzerinden doğrudan yürütme motoru devrededir.

6. **Canlı Veri (6.332 Parametre), DTC (18.246 Tanım) ve Freeze-Frame Desteği:**
   - CAN (`17 FF 00`, `19 02 FF`, vb.) ve K-Line (`18 00 FF 00`, `18 02 FF FF`, `14 FF 00`, vb.) DTC okuma ve silme rutinleri.
   - Freeze-Frame (`19 04`, `12`, `21 87`) arıza anı verileri çözümleyici.

7. **Modele Göre Gizli Özellik Açma & BCM Kişiselleştirme (5. Sekme — 21 Fonksiyon):**
   - `ReadBcmCodingStateJson()` ile CAN BCM (`0x7E4 -> 21 08 / 21 01`) ve K-Line BCM (`0x5A Pin 7 -> 21 08 / 21 01`) üzerinden aracın mevcut gizli özellik bitmask durumu okunur.
   - `ExecuteHiddenFeatureCoding()` ile seçilen araç modeline uygun protokol (CAN `30 <IOLI> 08 <CS>`, K-Line `10 C0 + 3B <LID> <VAL>`, UDS `10 03 + 2E <DID> <VAL>`) üzerinden kodlama yapılır:
     1. **`HF-DRL`:** Gündüz Sürüş Farı (DRL) AÇIK / KAPALI (`30 36 08 80`) — *Vitara, S-Cross, Swift, Baleno, Ignis, Jimny JB74*
     2. **`HF-AUTOLOCK`:** Hıza (15 km/s) veya Vitese (P'den Çıkınca) Duyarlı Otomatik Kapı Kilitleme (`30 35 08 01` + `3B 2F`) — *Grand Vitara JB, Swift, SX4, Vitara, S-Cross*
     3. **`HF-AUTOUNLOCK`:** Kontak Kapatınca (IG OFF) veya P Vitesinde Otomatik Kilit Açma (`30 35 08 02` + `3B 02`) — *Grand Vitara JB, Swift, SX4, Vitara, S-Cross*
     4. **`HF-LANEFLASH`:** Konfor Sinyali / Tek Dokunuş Şerit Değiştirme Sinyal Sayısı (`KAPALI` / `2` / `3` / `4 Kez` `30 35 08 20`) — *Vitara, S-Cross, Swift, Baleno, Ignis, Jimny*
     5. **`HF-MIRROR`:** Kumandadan Kilitleyince Otomatik Katlanır Yan Aynalar (`30 33 08 20`) — *Vitara, S-Cross, Swift, Baleno*
     6. **`HF-CORNER`:** Dönüşe Duyarlı Viraj Sis Farları / Cornering Light (`30 31 08 04`) — *Vitara, S-Cross, Swift, Baleno*
     7. **`HF-BUZZER`:** Kapı Kilitleme / Açma Sesli Onay (`Keyless Buzzer Bip` / `Sessiz` / `Korna` / `Siren` `30 33 08 80` + `3B 2B`) — *Grand Vitara JB, Swift, SX4, Vitara, S-Cross*
     8. **`HF-ALARM`:** Fabrika Hırsızlık Önleme (Security Alarm) Sistemi Aktivasyonu / İptali (`30 34 08 20` CAN + `30 09 08 20` K-Line `0x5A` + `3B 21`) — *Grand Vitara 1.9 DDiS / Benzinli, Swift, SX4, Vitara*
     9. **`HF-UNLOCKMODE`:** Kumandadan Kilit Açma Modu (`Tek Basışta Tüm Kapılar` vs `2 Aşamalı Önce Sürücü Kapısı` `30 3B 08 10`) — *Vitara, S-Cross, Swift, Grand Vitara CAN*
     10. **`HF-FOLLOWHOME`:** Follow Me Home (Eve Kadar Takip) Far Süresi (`0 sn` / `10 sn` / `15 sn` / `20 sn` / `25 sn` / `30 sn` `30 3B 08 02` + `3B 39`) — *Tüm CAN & K-Line BCM Modelleri*
     11. **`HF-GUIDECAR`:** Guide Me To Car (Kilit Açınca Karşılama Aydınlatması) Süresi (`0 sn..25 sn` `30 3B 08 04`) — *Vitara, S-Cross, Swift, Baleno, Ignis*
     12. **`HF-FOOTLIGHT`:** Ayak Altı (Footwell) Ambiyans Aydınlatma Modu (`Far+Kapı` / `Sadece Kapı` / `KAPALI` `30 3B 08 80`) — *Vitara, S-Cross, Swift, Baleno*
     13. **`HF-HBA`:** Otomatik Uzun Far Asistanı (`High Beam Assist` `30 36 08 40`) — *Vitara, S-Cross, Swift A2L, Jimny JB74*
     14. **`HF-ANSWERLIGHT`:** Kilitleme Görsel Onay Modu (`Dörtlü Sinyaller` vs `Sadece İç Tavan Lambası` `30 35 08 80` + `30 10 08 80`) — *Grand Vitara JB, Swift, SX4, Vitara*
     15. **`HF-AUTOLIGHT-HW`:** Otomatik Far Sensörü (`3B 3A 06`), Keyless (`3B 33 00`) & I-Key (`3B 34 01`) Sonradan Donanım (Retrofit) Kodlaması — *Grand Vitara JB 1.9 DDiS / Benzinli, Swift RS, SX4*
     16. **`HF-ROOMLAMP`:** İç Tavan Lambası Gecikmeli Sönme (Dimmer) Süresi (`0.5 sn` / `1 sn` / `2 sn` / `3 sn` / `0 sn` `3B 16` + `3B 17`) — *Grand Vitara JB, Swift, SX4, Jimny*
     17. **`HF-RETAINEDPWR`:** Kontak Kapandıktan Sonra Cam Kaldırma Yetkisi Süresi (`3B 27`) — *Grand Vitara JB, Swift, SX4, Jimny*
     18. **`HF-UDS-IKEY`:** UDS Akıllı Anahtar & Kapı Kolu Dokunmatik Tuş Kişiselleştirme (`2E 14 01..14`) — *Yeni Swift, Vitara Hybrid, S-Cross Hybrid*
     19. **`HF-SEATBELT`:** Emniyet Kemeri Sesli İkaz (Buzzer) Sesi Açma / Kapatma (`2E 20 41 00 60 / 00 00`) — *UDS BCM / Gösterge Modelleri*
     20. **`HF-CLUSTER-THEME`:** Gösterge / Bilgi Ekranı İkon Renk Teması Değişimi (`30 05 07 40` / `31 01 C0 11`) — *Swift, Vitara, S-Cross, Baleno*
     21. **`HF-ESP-DYNO`:** ESP Off-Road / Dyno Tam Devre Dışı Bırakma Modu (`30 80 07 04`) — *Grand Vitara JB (ESP), Swift Sport, SX4, Vitara*

8. **Immobilizer, Transponder & Keyless Anahtar Kodlama Modülü (`IMMO-01..207` — `POST /api/immo/action`):**
   - **Şifresiz (PIN Gerekmeden) Güvenli Okuma & Test İşlemleri:**
     - `read_status`: Kayıtlı anahtar sayısı (`keyCount`) ve kayıt yıl/ay bilgisini okuma (`21 A1`, `21 A0`, `21 01` — CAN `0x7E0`/`0x7E4` ve K-Line `0x5A`/`0x5B`/`0x11`).
     - `read_transponder`: Kontraktaki anahtarın 10-byte Transponder Çip ID kodunu okuma (`21 B0` — `IMMO-05`, `IMMO-06`, `IMMO-201`).
     - `read_pin`: Destekleyen beyinlerde doğrudan EEPROM PIN okuma (`10 C0 -> 21 21` — `IMMO-106`).
     - `antenna_test`: Keyless iç kabin anteni teşhis testi (`2F 01 06 03 00 05` — `BCM-167`).
   - **4 Haneli PIN Korumalı Anahtar & Kumanda Kodlama (Grand Vitara JB, Swift, SX4, Jimny — CAN & K-Line):**
     - Hazır Suzuki fabrika PIN seçicisi (`0895`, `9502`, `5634`, `0325`) ve özel 4 haneli PIN girişi.
     - `verify_pin`: Anahtarlara dokunmadan sadece girilen 4 haneli PIN kodunun ECU tarafından kabul edilip edilmediğini güvenli test etme (`30 A0 07 80 80 <PIN>` CAN / `A5 A0 80 00 <PIN>` K-Line).
     - `register_key`: Yeni Transponder Anahtar Kodlama (`30 A1 08 80 80` CAN / `A5 A1 80 00` K-Line / `31 A1 00` `IMMO-202` / `3B D2 00 01` `IMMO-203`).
     - `register_remote`: Uzaktan Kumanda (Remote Fob) Kodlama (`A5 A1 20 00` K-Line / `30 62 07` CAN `IMMO-204`).
     - `ecu_immo_init`: ECM <-> Immobilizer Beyin Eşleştirme / Başlatma (`31 01 <PIN>` — `IMMO-101`).
     - `steering_unlock`: Elektronik Direksiyon Kilidi (ESL) Açma (`31 0D 00 <PIN>` — `IMMO-109`).
     - `delete_all_keys`: Çift güvenlik onaylı Tüm Kayıtlı Anahtarları Silme (`30 A1 08 08` CAN / `A5 A1 08` K-Line).
   - **2015+ UDS Seed-Key Immobilizer (`IMMO-110..113` — Vitara LY, S-Cross, Yeni Swift):**
     - `uds_request_seed`: `10 03 -> 27 03` ile ECU'dan dinamik Seed alma.
     - `uds_unlock_register`: Hesaplanan Key ile kilidi açıp (`27 04 <KEY>`) anahtar kaydını başlatma (`31 01 01 0D`).

10. **Enjektör IMA Kalibrasyon Kodu (Silindir 1–4) Okuma/Yazma & 17 Hane Şasi No (VIN) Okuma/Yazma (Sekme 3):**
    - **Enjektör IMA Kodlama (`POST /api/injector/ima`):** Grand Vitara 1.9 DDiS (`F9Q`) ve CAN Dizel modellerde 4 silindirin 7 karakterlik IMA kalibrasyon kodlarını tek tıkla okuma (`21 AD` / `21 54` — CAN `0x7E0` ve K-Line `0x7A` yedekli) ve yeni enjektör kodlarını 28 byte ASCII olarak yazıp (`3B AD FF <28 ASCII>`) ardından ZFC sıfırlama (`3B 7E FE`) yapma.
    - **17 Haneli Şasi Numarası (VIN) Okuma & Yazma (`POST /api/vin`):** Çıkma veya sıfır ECM değişimlerinde `1A 90`, `22 F1 90`, `21 90`, `09 02` ile mevcut VIN numarasını okuma ve ECM yazma kilidini açarak (`30 12 08 80 80`, `30 76 08 80 80`, `A5 0F 40`) 17 haneli yeni şasi numarasını KWP (`3B 90`) ve UDS (`2E F1 90`) formatlarında kaydetme.

11. **4 Yeni Hızlı Kalibrasyon Kartı (EPB, HVAC, SHVS Hybrid, ADAS) + Canlı Veri CSV Kaydedici & HTML Arıza Raporu:**
    - `EPB-01`: Elektronik Park Freni Balata Değişim (Servis) Modu & Kalibrasyon (`A6 00 -> 30 01 07 -> 31 01 02`).
    - `HVAC-01`: Klima Hava Karışım Klepesi (Air Mix Flap) Aktüatör & Kompresör Başlatma (`0x7A2` CAN / `0x99` K-Line `30 01 08 80 80 -> 31 01 00`).
    - `HVBMU-01`: SHVS Mild-Hybrid / Strong Hybrid (ISG / Li-Ion Batarya BMU) Sıfırlama (`30 01 07 80 80` / `31 01 C0 10`).
    - `ADAS-01`: ADAS Ön Kamera / Radar / Kör Nokta (BSM) & 360 AVM Kalibrasyon Başlatma (`31 01 C0 01`).
    - **Yol Testi CSV Kaydedici (Sekme 2):** Canlı veri akışı sırasında tüm sensörleri zaman damgalı olarak kaydedip `.csv` (Excel) dosyası olarak indirme.
    - **Tek Tıkla HTML Arıza & Tarama Raporu (Sekme 1):** Taranan tüm ECU listesini, protokolleri, akü voltajını ve DTC arıza kodlarını `.html` raporu olarak dışa aktarma.

---

## 2. Doğrulama ve Test Sonuçları

- **Derleme Durumu:** Visual Studio 2022 MSVC x86 (`build_diag.cmd`) — **0 hata, 0 uyarı**.
- **Birim Testleri (`--check-identity`):** 4.053 kimlik kaydı, 570 servis fonksiyonu, 631 aktüatör testi, F9Q exact eşleşmesi, enum/ölçek dönüşümleri ve UDS DTC doğrulamaları **CHECK_EXIT=0** ile başarıyla geçti.
- **Yedekleme & Senkronizasyon:** `D:\Claude\suzuki` klasörü ile tüm proje dosyaları eşitlendi.

---

## 3. Dosya Yapısı ve Başlatma

| Dosya | Açıklama |
|---|---|
| `suzuki_diag_server.cpp` | C++ J2534 (ISO15765 CAN + ISO14230 K-Line) HTTP sunucusu ve teşhis motoru |
| `suzuki_diag_server.exe` | Derlenmiş 32-bit (x86) ana teşhis sunucusu |
| `index.html` | Çift dilli (EN varsayılan, TR seçilebilir), 5 sekmeli dinamik web arayüzü |
| `discovery-plan.js` | Modül keşif ve profil planlayıcısı |
| `suzuki_utilities.h`, `suzuki_utilities.generated.h` | 570 servis fonksiyonu, 23 OEM kalibrasyon motoru |
| `suzuki_active_tests.generated.h` | 631 aktüatör testi veri tablosu |
| `suzuki_identity.h`, `suzuki_identity.generated.h` | 4.053 ECU kimlik ve varyant eşleme tablosu |
| `suzuki_models.h`, `suzuki_models.generated.h` | 1.633 araç model tanımlaması |
| `suzuki_live.h`, `suzuki_live.generated.h` | 6.332 canlı veri parametresi ve 62 ECU tabanı adresleri |
| `suzuki_computed.h`, `suzuki_computed.generated.h` | 71 hesaplanmış parametre formül değerlendiricisi |
| `suzuki_dtc.h`, `suzuki_dtc.generated.h` | 18.246 arıza kodu tanımı |
| `suzuki_capture.h` | Freeze-Frame ve UDS DTC yanıt çözümleyici |
| `build_diag.cmd` | MSVC x86 derleme scripti |
| `Suzuki_Diyagnostik_Baslat.bat` | Tek tıkla sunucuyu ve tarayıcıyı başlatan script |
| `README.md` | GitHub için hazırlanan profesyonel İngilizce & Türkçe dokümantasyon |
| `.gitignore` | GitHub için güvenli dosya dışlama kuralları |

Sunucu Yerel Adresi: `http://127.0.0.1:8080`
