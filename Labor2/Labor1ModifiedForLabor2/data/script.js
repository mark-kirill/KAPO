// script.js — минимальная логика для index.html
function setColor(hex) {
  // hex = "#RRGGBB"
  const encoded = encodeURIComponent(hex);
  fetch('/set?value=' + encoded).then(resp => {
    // обновим UI
    document.getElementById('currentColor').textContent = hex;
    const picker = document.getElementById('picker');
    if (picker) picker.value = hex;
  }).catch(err => console.log('set error', err));
}

function submitHex(){
  const v = document.getElementById('hexInput').value.trim();
  if (!/^[0-9A-Fa-f]{6}$/.test(v)) { alert('Введите 6 hex символов'); return; }
  setColor('#' + v);
}

// On load — fetch status
function refreshStatus() {
  fetch('/status').then(r => r.json()).then(j => {
    if (!j.authenticated) {
      // not logged in -> go to login page
      window.location.href = '/login.html';
      return;
    }
    document.getElementById('currentColor').textContent = j.color;
    const picker = document.getElementById('picker');
    if (picker) picker.value = j.color;
  }).catch(err => {
    console.log('status error', err);
  });
}

document.addEventListener('DOMContentLoaded', function(){
  refreshStatus();
});
