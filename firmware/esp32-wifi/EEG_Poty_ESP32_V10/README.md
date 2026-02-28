# ESP32 WiFi Firmware (Access Point Mode)

Versão modificada do firmware para usar WiFi UDP ao invés de Bluetooth SPP. O ESP32 cria sua própria rede WiFi (Access Point).

## Arquitetura

```
                    ┌──────────────────────┐
                    │   Potyplex EEG       │
[Computador] ◄─────►│   Access Point       │
     ou             │                      │
[Celular]    ◄─UDP─►│   SSID: Potyplex-EEG │
                    │   IP: 192.168.4.1    │
                    │   Port: 12345        │
                    └──────────────────────┘
```

**Conecte diretamente no EEG - não precisa de roteador!**

## Configuração Padrão

| Parâmetro | Valor |
|-----------|-------|
| SSID | `Potyplex-EEG` |
| Senha | `eeg12345` |
| IP do ESP32 | `192.168.4.1` |
| Porta UDP | `12345` |

---

## Instalação

### 1. Instalar Bibliotecas (Arduino IDE)

Vá em **Sketch → Include Library → Manage Libraries** e instale:
- `Adafruit MPU6050`
- `Adafruit BusIO`
- `Adafruit Unified Sensor`

### 2. Configurar (opcional)

Edite `EEG_Poty_ESP32_Library_Definitions.h`:

```c
#define AP_SSID     "Potyplex-EEG"    // Nome da rede
#define AP_PASSWORD "eeg12345"         // Senha (min 8 chars)
#define AP_CHANNEL  1                  // Canal WiFi (1-13)
#define UDP_PORT    12345              // Porta UDP
```

### 3. Compilar e Upload

1. Abra Arduino IDE
2. Selecione placa: **ESP32 Dev Module**
3. Abra `EEG_Poty_ESP32_V10.ino`
4. Clique em Upload

### 4. Verificar

Abra Serial Monitor (115200 baud):

```
===========================================
  Potyplex EEG - WiFi Access Point Mode
===========================================
Creating network: Potyplex-EEG
[OK] Access Point started!
SSID: Potyplex-EEG
Password: eeg12345
ESP32 IP: 192.168.4.1
UDP Port: 12345
[OK] UDP server started
===========================================
  1. Connect to WiFi: Potyplex-EEG
  2. Send 'b' to 192.168.4.1:12345
===========================================
```

---

## Conectando e Recebendo Dados

### Linux - Teste Rápido

```bash
# 1. Conecte no WiFi "Potyplex-EEG" (senha: eeg12345)

# 2. Enviar comando e receber dados
nc -u 192.168.4.1 12345
# Digite 'b' + Enter para iniciar streaming
# Digite 's' + Enter para parar
```

### Celular

Use um app UDP (ex: "UDP Terminal" no Android):
1. Conecte no WiFi `Potyplex-EEG`
2. Configure IP: `192.168.4.1`, Port: `12345`
3. Envie `b` para iniciar

### Python - Cliente Completo

```python
#!/usr/bin/env python3
"""
Cliente para Potyplex EEG WiFi
"""
import socket

ESP32_IP = "192.168.4.1"
UDP_PORT = 12345

# Criar socket UDP
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(5.0)
sock.bind(('0.0.0.0', 0))  # Bind em porta aleatória

# Iniciar streaming
sock.sendto(b'b', (ESP32_IP, UDP_PORT))
print("Streaming iniciado!")

# Receber dados
while True:
    try:
        data, addr = sock.recvfrom(33)

        if len(data) == 33 and data[0] == 0xA0 and data[32] == 0xC0:
            sample_num = data[1]

            # Extrair 8 canais (24 bits cada, signed)
            channels = []
            for ch in range(8):
                offset = 2 + ch * 3
                raw = (data[offset] << 16) | (data[offset+1] << 8) | data[offset+2]
                if raw & 0x800000:  # Negativo (complemento de 2)
                    raw -= 0x1000000
                channels.append(raw)

            print(f"Sample {sample_num:3d}: CH1={channels[0]:8d} CH2={channels[1]:8d}")

    except socket.timeout:
        print("Timeout")
        break
    except KeyboardInterrupt:
        sock.sendto(b's', (ESP32_IP, UDP_PORT))
        print("Streaming parado")
        break

sock.close()
```

---

## Protocolo de Dados

Cada pacote tem **33 bytes**:

| Offset | Tamanho | Descrição |
|--------|---------|-----------|
| 0 | 1 | Header: `0xA0` |
| 1 | 1 | Contador (0-255) |
| 2-25 | 24 | 8 canais EEG (3 bytes cada, 24-bit signed) |
| 26-31 | 6 | Acelerômetro (3 eixos, 2 bytes cada) |
| 32 | 1 | Footer: `0xC0` |

**Taxa:** 250 amostras/segundo

---

## Comandos OpenBCI

| Comando | Descrição |
|---------|-----------|
| `b` | Iniciar streaming |
| `s` | Parar streaming |
| `v` | Soft reset (mostra info) |
| `?` | Query registradores |
| `d` | Configurações default |
| `1-8` | Desligar canal 1-8 |
| `!@#$%^&*` | Ligar canal 1-8 |

---

## LED Status

| Padrão | Significado |
|--------|-------------|
| 3 piscadas rápidas | AP iniciado OK |
| LED fixo | Erro ao criar AP |
| Piscando durante streaming | Enviando dados |

---

## Arquivos

| Arquivo | Descrição |
|---------|-----------|
| `EEG_Poty_ESP32_V10.ino` | Sketch principal |
| `EEG_Poty_ESP32_Library_Definitions.h` | Configurações WiFi/hardware |
| `EEG_Poty_ESP32_Library.h` | Header da biblioteca |
| `EEG_Poty_ESP32_Library.cpp` | Implementação |

---

## Troubleshooting

### Não encontra a rede WiFi
- Verifique Serial Monitor para erros
- Tente mudar `AP_CHANNEL` para outro canal (1-13)

### Não recebe dados
- Verifique se está conectado na rede `Potyplex-EEG`
- Confirme IP `192.168.4.1` e porta `12345`
- Envie `b` para iniciar streaming

### Cliente desconecta
- Timeout padrão é 30 segundos sem comandos
- Envie qualquer comando como keepalive
- Ou aumente `CLIENT_TIMEOUT_MS` em `_Definitions.h`

---

## Referências

- [ESP32 WiFi API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html)
- [OpenBCI Protocol](https://docs.openbci.com/Cyton/CytonSDK/)
