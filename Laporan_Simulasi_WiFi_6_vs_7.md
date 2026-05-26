# Laporan Komprehensif Simulasi Jaringan Indoor: Wi-Fi 6 vs Wi-Fi 7 (Single-Link & MLO)

Dokumen ini merangkum seluruh parameter teknis, konfigurasi arsitektural, profil antrean QoS, dan hasil ekstraksi performansi dari tiga skenario simulasi telekomunikasi nirkabel berbasis **ns-3.47**:
1. `indoor_wifi6.cc` (Standar 802.11ax)
2. `indoor_wifi7.cc` (Standar 802.11be - Single Link)
3. `indoor_wifi7_final.cc` (Standar 802.11be - Multi-Link Operation / MLO)

---

## 1. Spesifikasi Topologi dan Lingkungan Fisik (Physical Environment)
Seluruh simulasi dikondisikan pada environment yang **identik (apple-to-apple)** agar validasi komparasi performa pada *Medium Access Control (MAC)* dan *Physical Layer (PHY)* dapat dibenarkan secara empiris.

- **Dimensi Ruangan (Indoor):** 8.0 m x 8.0 m x 3.0 m.
- **Model Propagasi (Pathloss & Fading):** `HybridBuildingsPropagationLossModel` (Sesuai dengan rekomendasi ITU-R P.1238 untuk perambatan ruang tertutup pada frekuensi SHF 5 GHz dan 6 GHz).
- **Material Penghalang:** Dinding berbahan beton tanpa jendela (*Concrete Without Windows*) untuk menghasilkan redaman (atenuasi) yang realistis terhadap propagasi sinyal.
- **Node Telekomunikasi:**
  - **1 Access Point (AP):** Ditempatkan di titik tengah langit-langit berkoordinat `(X=4.0, Y=4.0, Z=2.9)`.
  - **5 User Stations (STA):** Disebar secara seragam (Uniform Random) pada sumbu X dan Y dengan rentang 0.5 - 7.5 m, pada ketinggian *user level* `Z=1.0`.

---

## 2. Profil Trafik Pengguna dan QoS (WMM EDCA)
Beban trafik (*traffic load*) dikonstruksi menggunakan `OnOffApplication` UDP secara paralel (*bursty traffic*) untuk mengevaluasi saturasi kanal (total *aggregated throughput* = **3.9 Gbps**). Klasifikasi antrean diatur berdasarkan **IP Type of Service (ToS)** yang dipetakan langsung ke **WMM EDCA Access Categories (AC)**.

| Nama Aplikasi / Profil | IP ToS | Kategori WMM (EDCA) | Target Data Rate | Ukuran Paket (Payload) | Kategori ITU-T G.1010 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Social Media** | `0x00` | **AC_BE** (Best Effort) | 200 Mbps | 200 Bytes | *Interactive Web* |
| **Video 4K Streaming** | `0xa0` | **AC_VI** (Video) | 1500 Mbps | 1472 Bytes | *Streaming Media* |
| **Gaming (Cloud)** | `0xc0` | **AC_VO** (Voice/High Priority)| 500 Mbps | 1472 Bytes | *Highly Interactive* |
| **File Download** | `0x20` | **AC_BK** (Background) | 1500 Mbps | 1472 Bytes | *Bulk Data Transfer* |
| **Web Browsing** | `0x00` | **AC_BE** (Best Effort) | 200 Mbps | 1000 Bytes | *Interactive Web* |

---

## 3. Spesifikasi Arsitektur per Standar Wi-Fi

### A. Skenario 1: Wi-Fi 6 (`indoor_wifi6.cc`)
Menguji kapabilitas pita tunggal lebar dari standar **IEEE 802.11ax**.
- **Standar PHY/MAC:** `WIFI_STANDARD_80211ax`
- **Konfigurasi Kanal:** Spektrum 6 GHz, *Bandwidth* 160 MHz.
- **Antena & Spasial:** MIMO 4x4 (4 Tx, 4 Rx Spatial Streams).
- **Buffer A-MPDU (Block Ack Window):** Terbatas pada 256 MPDU.
- **Hasil Kinerja (*Output*):**
  Pada beban 3.9 Gbps, *buffer* 256 MPDU menyebabkan *MAC queue bloat* (kemacetan antrean). *Throughput* runtuh dengan tingkat *Packet Loss* meroket hingga puluhan persen pada kelas trafik seperti *File Download*, dan *average delay* mencapai tingkat makro (ratusan hingga ribuan milidetik).

### B. Skenario 2: Wi-Fi 7 Single-Link (`indoor_wifi7.cc`)
Implementasi standar baru **IEEE 802.11be** namun masih beroperasi pada infrastruktur frekuensi tradisional (Pita tunggal).
- **Standar PHY/MAC:** `WIFI_STANDARD_80211be`
- **Konfigurasi Kanal:** Spektrum 6 GHz, *Bandwidth* 160 MHz.
- **Antena & Spasial:** MIMO 4x4.
- **Buffer A-MPDU (Block Ack Window):** Dioptimasi menjadi 1024 MPDU (Memanfaatkan agregasi ekstrem Wi-Fi 7).
- **Hasil Kinerja (*Output*):**
  Peningkatan ukuran agregat A-MPDU ke 1024 membuat efisiensi efektivitas PHY meningkat drastis. Simulasi mampu menyerap seluruh 3.9 Gbps dengan tingkat *Packet Loss* nyaris nol dan *delay* direduksi hingga kisaran sub-milidetik (~0.3 ms).

### C. Skenario 3: Wi-Fi 7 MLO (`indoor_wifi7_final.cc`)
Titik kulminasi dari Wi-Fi 7. Mengimplementasikan **Multi-Link Operation (MLO)** bertipe **STR (Simultaneous Transmit and Receive)** yang menggabungkan beberapa pita spektral menjadi satu jembatan virtual transmisi.
- **Standar PHY/MAC:** `WIFI_STANDARD_80211be` (Berjalan sebagai perangkat MLD / *Multi-Link Device*).
- **Konfigurasi Kanal:** **Total Agregat 480 MHz**
  - *Link 0:* Spektrum 5 GHz, *Bandwidth* 160 MHz (Channel Auto-Index).
  - *Link 1:* Spektrum 6 GHz, *Bandwidth* 320 MHz (Channel Auto-Index).
- **Antena & Spasial:** MIMO 4x4 pada masing-masing Link.
- **Buffer A-MPDU:** 1024 MPDU.
- **Hasil Kinerja (*Output*):**
  Hasil spektakuler pada beban konstan 3.9 Gbps. Karena kapasitas medium diekspansi menjadi 480 MHz, *channel utilization* sangat renggang. Seluruh paket aplikasi dihantarkan ke tujuan dengan probabilitas keberhasilan nyaris absolut (**Packet Loss 0.05% - 0.7%**), tanpa terjadinya fenomena *EDCA starvation*, serta latensi (*average delay*) teredam di tingkat dasar (~0.2 milidetik).

---

## 4. Kesimpulan Teknis

Skrip simulasi ini berhasil memvalidasi secara saintifik bahwa inovasi arsitektur **IEEE 802.11be** (terutama Agregasi 1024 A-MPDU, kapabilitas *320 MHz Channel*, dan integrasi level-MAC dari **MLO STR**) menyediakan lonjakan kapasitas linear terhadap kapabilitas teoretis spektralnya. Sementara standar turunan sebelumnya (Wi-Fi 6) tersumbat pada beban agregasi 4 Gbps di lingkungan *indoor*, Wi-Fi 7 MLO mengasimilasinya tanpa memprovokasi degradasi QoS.
