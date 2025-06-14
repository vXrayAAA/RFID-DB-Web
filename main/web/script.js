// Variáveis globais
let currentScanningCard = false;
let scanInterval = null;

// Inicialização da página
document.addEventListener('DOMContentLoaded', function() {
    initializeApp();
    setupEventListeners();
    loadData();
    
    // Atualizar dados a cada 30 segundos
    setInterval(loadData, 30000);
});

// Inicializar aplicação
function initializeApp() {
    updateConnectionStatus();
    showToast('Sistema RFID carregado com sucesso!', 'info');
}

// Configurar event listeners
function setupEventListeners() {
    // Formulário de adicionar cartão
    document.getElementById('add-card-form').addEventListener('submit', handleAddCard);
    
    // Botão de escanear cartão
    document.getElementById('scan-card-btn').addEventListener('click', toggleCardScan);
    
    // Botão de atualizar logs
    document.getElementById('refresh-logs-btn').addEventListener('click', loadAccessLogs);
    
    // Modal
    document.getElementById('modal-cancel').addEventListener('click', hideModal);
    document.getElementById('modal').addEventListener('click', function(e) {
        if (e.target === this) hideModal();
    });
}

// Carregar todos os dados
async function loadData() {
    try {
        await Promise.all([
            loadStats(),
            loadCards(),
            loadAccessLogs()
        ]);
        updateConnectionStatus(true);
        updateLastUpdate();
    } catch (error) {
        console.error('Erro ao carregar dados:', error);
        updateConnectionStatus(false);
        showToast('Erro ao conectar com o ESP32', 'error');
    }
}

// Carregar estatísticas
async function loadStats() {
    try {
        const response = await fetch('/api/stats');
        const data = await response.json();
          document.getElementById('total-cards').textContent = data.total_cards || 0;
        document.getElementById('total-accesses').textContent = data.total_accesses || 0;
        
        // Buscar último cartão usando endpoint específico
        try {
            const lastCardResponse = await fetch('/api/last_card');
            const lastCardData = await lastCardResponse.json();
            if (lastCardData.success && lastCardData.uid) {
                document.getElementById('last-card').textContent = `${lastCardData.name || lastCardData.uid} (${lastCardData.uid})`;
            } else {
                document.getElementById('last-card').textContent = 'Nenhum cartão';
            }
        } catch (error) {
            console.error('Erro ao buscar último cartão:', error);
            document.getElementById('last-card').textContent = 'Nenhum cartão';
        }
    } catch (error) {
        console.error('Erro ao carregar estatísticas:', error);
        throw error;
    }
}

// Carregar cartões
async function loadCards() {
    try {
        const response = await fetch('/api/cards');
        const data = await response.json();
        
        const tbody = document.querySelector('#cards-table tbody');
        tbody.innerHTML = '';
        
        if (data.cards && data.cards.length > 0) {
            data.cards.forEach(card => {
                const row = createCardRow(card);
                tbody.appendChild(row);
            });
        } else {
            tbody.innerHTML = '<tr><td colspan="7" style="text-align: center;">Nenhum cartão cadastrado</td></tr>';
        }
    } catch (error) {
        console.error('Erro ao carregar cartões:', error);
        throw error;
    }
}

// Criar linha da tabela de cartões
function createCardRow(card) {
    const row = document.createElement('tr');
    
    const accessLevel = getAccessLevelBadge(card.access_level);
    const firstSeen = formatDate(card.first_seen);
    const lastSeen = formatDate(card.last_seen);
    
    row.innerHTML = `
        <td><code>${card.uid}</code></td>
        <td>${card.name}</td>
        <td>${accessLevel}</td>
        <td>${firstSeen}</td>
        <td>${lastSeen}</td>
        <td>${card.access_count}</td>
        <td>
            <button class="btn-danger" onclick="deleteCard('${card.uid}', '${card.name}')">
                🗑️ Excluir
            </button>
        </td>
    `;
    
    return row;
}

// Obter badge do nível de acesso
function getAccessLevelBadge(level) {
    const levels = {
        1: { text: 'Básico', class: 'level-1' },
        2: { text: 'Intermediário', class: 'level-2' },
        3: { text: 'Administrador', class: 'level-3' }
    };
    
    const levelInfo = levels[level] || levels[1];
    return `<span class="access-badge ${levelInfo.class}">${levelInfo.text}</span>`;
}

// Carregar logs de acesso
async function loadAccessLogs() {
    try {
        const response = await fetch('/api/logs');
        const data = await response.json();
        
        const tbody = document.querySelector('#access-log-table tbody');
        tbody.innerHTML = '';
        
        if (data.logs && data.logs.length > 0) {
            data.logs.forEach(log => {
                const row = createLogRow(log);
                tbody.appendChild(row);
            });
        } else {
            tbody.innerHTML = '<tr><td colspan="3" style="text-align: center;">Nenhum log de acesso</td></tr>';
        }
    } catch (error) {
        console.error('Erro ao carregar logs:', error);
        throw error;
    }
}

// Criar linha da tabela de logs
function createLogRow(log) {
    const row = document.createElement('tr');
    
    const timestamp = formatDateTime(log.timestamp);
    const actionIcon = log.action === 'ACCESS' ? '✅' : '❌';
    
    row.innerHTML = `
        <td>${timestamp}</td>
        <td><code>${log.uid}</code></td>
        <td>${actionIcon} ${log.action}</td>
    `;
    
    return row;
}

// Manipular adição de cartão
async function handleAddCard(event) {
    event.preventDefault();
    
    const uid = document.getElementById('card-uid').value.trim();
    const name = document.getElementById('card-name').value.trim();
    const accessLevel = parseInt(document.getElementById('access-level').value);
    
    if (!uid || !name) {
        showToast('Por favor, preencha todos os campos', 'warning');
        return;
    }
    
    try {
        const response = await fetch('/api/cards', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                uid: uid,
                name: name,
                access_level: accessLevel
            })
        });
        
        if (response.ok) {
            showToast('Cartão adicionado com sucesso!', 'success');
            document.getElementById('add-card-form').reset();
            await loadData();
        } else {
            const error = await response.json();
            showToast(error.message || 'Erro ao adicionar cartão', 'error');
        }
    } catch (error) {
        console.error('Erro ao adicionar cartão:', error);
        showToast('Erro de conexão', 'error');
    }
}

// Alternar escaneamento de cartão
async function toggleCardScan() {
    const button = document.getElementById('scan-card-btn');
    const uidInput = document.getElementById('card-uid');
    
    if (currentScanningCard) {
        // Parar escaneamento
        currentScanningCard = false;
        clearInterval(scanInterval);
        button.textContent = '📱 Aproximar Cartão';
        button.classList.remove('btn-primary');
        button.classList.add('btn-secondary');
        uidInput.disabled = false;
    } else {
        // Iniciar escaneamento
        currentScanningCard = true;
        button.textContent = '⏹️ Parar Scan';
        button.classList.remove('btn-secondary');
        button.classList.add('btn-primary');
        uidInput.disabled = true;
        uidInput.value = 'Aguardando cartão...';
        
        showToast('Aproxime um cartão RFID do leitor', 'info');
        
        // Verificar por novo cartão a cada 1 segundo
        scanInterval = setInterval(async () => {
            try {
                const response = await fetch('/api/scan');
                const data = await response.json();
                
                if (data.success && data.uid) {
                    // Cartão detectado
                    uidInput.value = data.uid;
                    currentScanningCard = false;
                    clearInterval(scanInterval);
                    button.textContent = '📱 Aproximar Cartão';
                    button.classList.remove('btn-primary');
                    button.classList.add('btn-secondary');
                    uidInput.disabled = false;
                    
                    showToast(`Cartão detectado: ${data.uid}`, 'success');
                }
            } catch (error) {
                console.error('Erro ao escanear cartão:', error);
            }
        }, 1000);
    }
}

// Deletar cartão
function deleteCard(uid, name) {
    showModal(
        'Confirmar Exclusão',
        `Tem certeza que deseja excluir o cartão "${name}" (${uid})?`,
        async () => {
            try {
                const response = await fetch(`/api/cards/${encodeURIComponent(uid)}`, {
                    method: 'DELETE'
                });
                
                if (response.ok) {
                    showToast('Cartão excluído com sucesso!', 'success');
                    await loadData();
                } else {
                    const error = await response.json();
                    showToast(error.message || 'Erro ao excluir cartão', 'error');
                }
            } catch (error) {
                console.error('Erro ao excluir cartão:', error);
                showToast('Erro de conexão', 'error');
            }
            hideModal();
        }
    );
}

// Mostrar modal
function showModal(title, message, confirmCallback) {
    document.getElementById('modal-title').textContent = title;
    document.getElementById('modal-message').textContent = message;
    document.getElementById('modal').style.display = 'block';
    
    // Remover listeners anteriores
    const confirmBtn = document.getElementById('modal-confirm');
    const newConfirmBtn = confirmBtn.cloneNode(true);
    confirmBtn.parentNode.replaceChild(newConfirmBtn, confirmBtn);
    
    // Adicionar novo listener
    newConfirmBtn.addEventListener('click', confirmCallback);
}

// Esconder modal
function hideModal() {
    document.getElementById('modal').style.display = 'none';
}

// Mostrar toast
function showToast(message, type = 'success') {
    const toast = document.getElementById('toast');
    const messageElement = document.getElementById('toast-message');
    
    messageElement.textContent = message;
    toast.className = `toast ${type}`;
    toast.classList.add('show');
    
    setTimeout(() => {
        toast.classList.remove('show');
    }, 3000);
}

// Atualizar status de conexão
function updateConnectionStatus(connected = false) {
    const statusElement = document.getElementById('connection-status');
    
    if (connected) {
        statusElement.textContent = '🟢 Conectado';
        statusElement.classList.add('connected');
    } else {
        statusElement.textContent = '🔴 Desconectado';
        statusElement.classList.remove('connected');
    }
}

// Atualizar última atualização
function updateLastUpdate() {
    const now = new Date();
    const timeString = now.toLocaleTimeString('pt-BR');
    document.getElementById('last-update').textContent = `Última atualização: ${timeString}`;
}

// Formatar data
function formatDate(timestamp) {
    if (!timestamp) return '--';
    const date = new Date(timestamp * 1000);
    return date.toLocaleDateString('pt-BR');
}

// Formatar data e hora
function formatDateTime(timestamp) {
    if (!timestamp) return '--';
    const date = new Date(timestamp * 1000);
    return `${date.toLocaleDateString('pt-BR')} ${date.toLocaleTimeString('pt-BR')}`;
}

// Função para testar a API (desenvolvimento)
async function testAPI() {
    try {
        console.log('Testando API...');
        
        // Testar stats
        const statsResponse = await fetch('/api/stats');
        const statsData = await statsResponse.json();
        console.log('Stats:', statsData);
        
        // Testar cards
        const cardsResponse = await fetch('/api/cards');
        const cardsData = await cardsResponse.json();
        console.log('Cards:', cardsData);
        
        // Testar logs
        const logsResponse = await fetch('/api/logs');
        const logsData = await logsResponse.json();
        console.log('Logs:', logsData);
        
    } catch (error) {
        console.error('Erro no teste da API:', error);
    }
}

// Expor função de teste no console (apenas para desenvolvimento)
window.testAPI = testAPI;
