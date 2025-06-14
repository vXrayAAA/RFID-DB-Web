# Sistema RFID com ESP32-S3 📡Sistema completo de controle de acesso RFID utilizando ESP32-S3, módulo RC522 e interface web responsiva. O projeto implementa um banco de dados local usando NVS (Non-Volatile Storage) e oferece uma API REST completa para gerenciamento de cartões.## 🚀 Características- **Detecção automática de cartões RFID** - Reconhece qualquer cartão automaticamente- **Interface web moderna** - Dashboard responsivo para gerenciamento- **API REST completa** - Endpoints para todas as operações CRUD- **Banco de dados local** - Armazenamento persistente usando NVS do ESP32- **Wi-Fi Station Mode** - Conecta automaticamente à rede configurada- **Logs detalhados** - Sistema de debug completo para diagnóstico- **Arquitetura modular** - Código organizado em componentes reutilizáveis## 🛠️ Hardware Necessário- **ESP32-S3** (placa de desenvolvimento)- **Módulo RC522** (leitor RFID)- **Cartões/Tags RFID** (13.56MHz)- **Jumpers** para conexão- **Fonte 5V** (opcional, dependendo da placa)## 📋 Conexões### RC522 ↔ ESP32-S3| RC522 | ESP32-S3 | Descrição ||-------|----------|-----------|| MISO  | GPIO 35  | Master In Slave Out || MOSI  | GPIO 36  | Master Out Slave In || SCK   | GPIO 37  | Serial Clock || SDA   | GPIO 39  | Chip Select || RST   | GPIO 38  | Reset |
| 3.3V  | 3.3V     | Alimentação |
| GND   | GND      | Terra |

## ⚙️ Configuração

### 1. Configuração Wi-Fi

Edite o arquivo `main/wifi_manager.h`:

```c
#define WIFI_SSID "SUA_REDE_WIFI"
#define WIFI_PASSWORD "SUA_SENHA"
```

### 2. Build e Flash

```bash
# Configure o projeto
idf.py menuconfig

# Compile o projeto
idf.py build

# Flash no ESP32
idf.py -p COMx flash monitor
```

### 3. Acesso à Interface

Após a inicialização, acesse:
- **Interface Web**: `http://IP_DO_ESP32`
- **API REST**: `http://IP_DO_ESP32/api/`

## 🌐 API REST

### Endpoints Disponíveis

| Método | Endpoint | Descrição |
|--------|----------|-----------|
| GET | `/api/cards` | Lista todos os cartões |
| GET | `/api/last_card` | Último cartão detectado |
| POST | `/api/cards` | Adiciona novo cartão |
| PUT | `/api/cards/{id}` | Atualiza cartão existente |
| DELETE | `/api/cards/{id}` | Remove cartão |

### Exemplos de Uso

```bash
# Listar cartões
curl http://192.168.1.100/api/cards

# Obter último cartão detectado
curl http://192.168.1.100/api/last_card

# Adicionar cartão
curl -X POST http://192.168.1.100/api/cards \
  -H "Content-Type: application/json" \
  -d '{"uid":"04:A3:B2:C1","name":"João Silva","access_level":1}'
```

## 📁 Estrutura do Projeto

```
RFID-Database/
├── main/
│   ├── main.c              # Código principal
│   ├── rc522.c/h           # Driver do módulo RFID
│   ├── database_new.c      # Banco de dados NVS
│   ├── database.h          # Estruturas de dados
│   ├── web_server.c/h      # Servidor HTTP
│   ├── wifi_manager.c/h    # Gerenciador Wi-Fi
│   ├── web/                # Interface web
│   │   ├── index.html      # Página principal
│   │   ├── script.js       # JavaScript
│   │   └── style.css       # Estilos CSS
│   └── CMakeLists.txt      # Configuração de build
├── CMakeLists.txt          # Configuração principal
└── README.md              # Esta documentação
```

## 🔧 Funcionalidades Técnicas

### Sistema de Banco de Dados

- **Armazenamento**: NVS (Non-Volatile Storage)
- **Estrutura**: Chaves numéricas para otimização
- **Capacidade**: Limitada pela memória flash disponível
- **Persistência**: Dados mantidos entre reinicializações

### Comunicação RFID

- **Protocolo**: SPI com timing otimizado
- **Frequência**: 13.56MHz (ISO14443A)
- **Alcance**: ~3cm (dependente da antena)
- **Auto-detecção**: Sistema reconhece qualquer UID

### Interface Web

- **Design**: Responsivo e moderno
- **Framework**: HTML5, CSS3, JavaScript vanilla
- **Recursos**: 
  - Dashboard em tempo real
  - Gerenciamento de cartões
  - Logs de acesso
  - Status do sistema

## 🐛 Troubleshooting

### Problemas Comuns

1. **Cartão não detectado**
   - Verifique as conexões SPI
   - Confirme a alimentação do RC522
   - Teste com diferentes cartões

2. **Falha na conexão Wi-Fi**
   - Confirme credenciais no código
   - Verifique sinal da rede
   - Monitore logs via serial

3. **Interface web não carrega**
   - Confirme IP do ESP32
   - Verifique se o servidor web iniciou
   - Teste endpoints da API diretamente

### Logs de Debug

O sistema possui logs detalhados acessíveis via monitor serial:

```bash
idf.py monitor
```

## 🚀 Próximas Melhorias

- [ ] Autenticação de usuários
- [ ] Banco de dados remoto
- [ ] Notificações push
- [ ] Controle de relés/fechaduras
- [ ] Histórico de acessos expandido
- [ ] Interface mobile nativa

## 📄 Licença

Este projeto está sob a licença MIT. Veja o arquivo LICENSE para mais detalhes.

## 🤝 Contribuição

Contribuições são bem-vindas! Por favor:

1. Faça um fork do projeto
2. Crie uma branch para sua feature
3. Commit suas mudanças
4. Push para a branch
5. Abra um Pull Request

## 👤 Autor

Desenvolvido com ❤️ para a comunidade IoT.

---

**💡 Dica**: Este projeto é ideal para iniciantes em ESP32 e IoT, mas também oferece recursos avançados para projetos profissionais de controle de acesso.
