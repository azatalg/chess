const symbols={K:'♔',Q:'♕',R:'♖',B:'♗',N:'♘',P:'♙',k:'♚',q:'♛',r:'♜',b:'♝',n:'♞',p:'♟'};
const board=document.querySelector('#board'),status=document.querySelector('#status'),lobby=document.querySelector('#lobby');
const botSelect=document.querySelector('#bot'),timeSelect=document.querySelector('#time'),roomBadge=document.querySelector('#roomBadge');
const localSession=sessionStorage.getItem('chessSession')||(crypto.randomUUID?crypto.randomUUID():`${Date.now()}-${Math.random()}`);sessionStorage.setItem('chessSession',localSession);
const sessionQuery=`session=${encodeURIComponent(localSession)}`;
let state,selected=null,mode='menu',socket=null,player=null,roomAction=null,flipped=false,autoplayToken=0,roomRefreshTimer=null,toastTimer=null,busy=false;
const squareName=i=>String.fromCharCode(97+i%8)+(Math.floor(i/8)+1);
const resultText=r=>({ongoing:'',white_wins:'Mat — wygrywają białe',black_wins:'Mat — wygrywają czarne',stalemate:'Pat',fifty_move_draw:'Remis — 50 ruchów',repetition_draw:'Remis — powtórzenie',insufficient_material:'Remis — brak materiału'})[r]||r;
function showToast(message,error=false){const toast=document.querySelector('#toast');clearTimeout(toastTimer);toast.textContent=message;toast.classList.toggle('error',error);toast.classList.add('visible');toastTimer=setTimeout(()=>toast.classList.remove('visible'),2600);}
function setBusy(value,label){busy=value;const submit=document.querySelector('#roomSubmit');submit.disabled=value;if(label){if(!submit.dataset.label)submit.dataset.label=submit.textContent;submit.textContent=value?label:submit.dataset.label;}document.querySelector('#roomCancel').disabled=value;}
function stopRoomRefresh(){if(roomRefreshTimer){clearInterval(roomRefreshTimer);roomRefreshTimer=null;}}

function render(){
  if(!state)return; board.innerHTML='';
  const legalTargets=new Set(selected===null?[]:state.legal.filter(m=>m.startsWith(squareName(selected))).map(m=>m.slice(2,4)));
  const ranks=flipped?[0,1,2,3,4,5,6,7]:[7,6,5,4,3,2,1,0];
  const files=flipped?[7,6,5,4,3,2,1,0]:[0,1,2,3,4,5,6,7];
  for(let row=0;row<8;row++)for(let column=0;column<8;column++){
    const rank=ranks[row],file=files[column];
    const i=rank*8+file,square=document.createElement('div');square.className=`square ${(file+rank)%2?'dark':'light'}`;square.dataset.file=String.fromCharCode(97+file);square.dataset.rank=rank+1;
    if(column===0)square.dataset.edgeRank=rank+1;if(row===7)square.dataset.edgeFile=String.fromCharCode(97+file);
    if(i===selected)square.classList.add('selected');if(legalTargets.has(squareName(i)))square.classList.add('legal');square.dataset.index=i;
    const p=state.board[i];if(p){const piece=document.createElement('span');piece.className=`piece ${p===p.toUpperCase()?'white-piece':'black-piece'}`;piece.textContent=symbols[p];piece.draggable=true;piece.addEventListener('dragstart',e=>{if(!canControl(i)){e.preventDefault();return;}selected=i;e.dataTransfer.setData('text/plain',i);render();});square.append(piece);}
    square.addEventListener('dragover',e=>e.preventDefault());square.addEventListener('drop',e=>{e.preventDefault();tryMove(Number(e.dataTransfer.getData('text/plain')),i);});square.addEventListener('click',()=>selected===null?select(i):tryMove(selected,i));board.append(square);
  }
  const waiting=mode==='online'&&window.onlinePlayers<2;
  status.textContent=waiting?'Czekam na rywala…':state.result==='ongoing'?`${state.turn==='white'?'Białe':'Czarne'}${state.check?' — szach!':''}`:resultText(state.result);
}
function canControl(i){if(!state.board[i])return false;if(mode!=='online')return true;const isWhite=state.board[i]===state.board[i].toUpperCase();return window.onlinePlayers===2&&state.turn===player&&isWhite===(player==='white');}
function select(i){if(canControl(i)){selected=i;render();}}
function applyOptimisticMove(from,to,notation){
  const piece=state.board[from],target=state.board[to];
  state.board[from]=null;
  if(piece&&piece.toLowerCase()==='p'&&!target&&(from%8)!==(to%8))state.board[to+(piece==='P'?-8:8)]=null;
  if(piece&&piece.toLowerCase()==='k'&&Math.abs((from%8)-(to%8))===2){
    const kingSide=to>from,rookFrom=kingSide?from+3:from-4,rookTo=kingSide?from+1:from-1;
    state.board[rookTo]=state.board[rookFrom];state.board[rookFrom]=null;
  }
  let moved=piece;
  if(notation.length>4){moved=notation[4];if(piece===piece.toUpperCase())moved=moved.toUpperCase();}
  state.board[to]=moved;state.turn=state.turn==='white'?'black':'white';state.check=false;state.legal=[];selected=null;render();
}
async function tryMove(from,to){
  if(from===null||!canControl(from)){selected=null;render();return;}const base=squareName(from)+squareName(to);const legal=state.legal.find(m=>m===base)||state.legal.find(m=>m.startsWith(base));if(!legal){selected=null;render();return;}selected=null;
  applyOptimisticMove(from,to,legal);
  if(mode==='online'){status.textContent='Ruch wysłany…';socket.send(`move:${legal}`);return;}status.textContent='Bot myśli…';const response=await fetch(`/api/move?move=${legal}&${sessionQuery}`,{method:'POST'});if(response.ok){state=await response.json();render();}else await load();
}
async function load(){state=await fetch(`/api/state?${sessionQuery}`).then(r=>r.json());selected=null;render();}
function showGame(){stopRoomRefresh();lobby.classList.add('hidden');document.body.classList.remove('lobby-open');}
function showLobby(){autoplayToken++;stopRoomRefresh();if(socket){socket.close();socket=null;}mode='menu';player=null;roomBadge.classList.add('hidden');lobby.classList.remove('hidden');document.body.classList.add('lobby-open');document.querySelector('#roomForm').classList.add('hidden');document.querySelector('.mode-grid').classList.remove('hidden');setBusy(false);}
const wait=milliseconds=>new Promise(resolve=>setTimeout(resolve,milliseconds));
async function runAutoplay(){
  const token=++autoplayToken;
  while(token===autoplayToken&&mode==='solo'&&botSelect.value==='both'&&state?.result==='ongoing'){
    await wait(180);if(token!==autoplayToken||mode!=='solo')break;
    status.textContent='Bot analizuje…';
    try{const response=await fetch(`/api/bot-step?${sessionQuery}`,{method:'POST'});if(!response.ok)break;state=await response.json();selected=null;render();}catch{status.textContent='Przerwano autoplay';break;}
  }
}
async function startSolo(){mode='solo';flipped=botSelect.value==='white';botSelect.disabled=false;timeSelect.disabled=false;document.querySelector('#newGame').classList.remove('hidden');showGame();state=await fetch(`/api/new?bot=${botSelect.value}&time=${timeSelect.value}&${sessionQuery}`,{method:'POST'}).then(r=>r.json());render();if(botSelect.value==='both')runAutoplay();}
async function refreshRooms(){
  const list=document.querySelector('#roomList');list.innerHTML='<p>Wyszukiwanie pokojów…</p>';
  try{const rooms=await fetch('/api/rooms').then(r=>{if(!r.ok)throw new Error();return r.json();});if(!rooms.length){list.innerHTML='<p>Brak wolnych pokojów. Utwórz pierwszy.</p>';return;}list.innerHTML='';for(const room of rooms){const button=document.createElement('button');button.type='button';button.className='room-entry';button.innerHTML=`<b>${room.code}</b><small>${room.players}/2 GRACZY</small>`;button.onclick=()=>{document.querySelectorAll('.room-entry').forEach(e=>e.classList.remove('selected'));button.classList.add('selected');document.querySelector('#roomCode').value=room.code;document.querySelector('#roomPassword').focus();};list.append(button);}}catch{list.innerHTML='<p>Nie udało się pobrać listy.</p>';}
}
function openRoomForm(action){stopRoomRefresh();roomAction=action;document.querySelector('.mode-grid').classList.add('hidden');document.querySelector('#roomForm').classList.remove('hidden');document.querySelector('#codeField').classList.toggle('hidden',action==='create');document.querySelector('#roomBrowser').classList.toggle('hidden',action==='create');const submit=document.querySelector('#roomSubmit');submit.textContent=action==='create'?'UTWÓRZ POKÓJ':'DOŁĄCZ';submit.dataset.label=submit.textContent;document.querySelector('#lobbyError').textContent='';if(action==='join'){refreshRooms();roomRefreshTimer=setInterval(refreshRooms,5000);}setTimeout(()=>document.querySelector(action==='join'?'#roomCode':'#roomPassword').focus(),0);}
function connectRoom(code,password){
  const protocol=location.protocol==='https:'?'wss':'ws';socket=new WebSocket(`${protocol}://${location.host}/ws?room=${encodeURIComponent(code)}&password=${encodeURIComponent(password)}`);
  const timeout=setTimeout(()=>{if(socket&&socket.readyState===WebSocket.CONNECTING)socket.close();},8000);
  socket.onopen=()=>{clearTimeout(timeout);setBusy(false);mode='online';botSelect.disabled=true;timeSelect.disabled=true;document.querySelector('#newGame').classList.add('hidden');showGame();status.textContent='Synchronizacja…';showToast(`Połączono z pokojem ${code}`);};
  socket.onmessage=e=>{const msg=JSON.parse(e.data);if(msg.type==='state'){state=msg.game;if(player===null)flipped=msg.player==='black';player=msg.player;window.onlinePlayers=msg.players;roomBadge.dataset.code=msg.room;roomBadge.textContent=`ROOM ${msg.room} // ${player==='white'?'BIAŁE':'CZARNE'} // ${msg.players}/2`;roomBadge.classList.remove('hidden');selected=null;render();}else if(msg.type==='error'){status.textContent=msg.message;showToast(msg.message,true);}};
  socket.onerror=()=>{clearTimeout(timeout);setBusy(false);document.querySelector('#lobbyError').textContent='Nie udało się połączyć. Sprawdź kod i hasło.';showToast('Nie udało się połączyć z pokojem',true);if(mode==='online')showLobby();};socket.onclose=()=>{clearTimeout(timeout);setBusy(false);if(mode==='online'){status.textContent='Rozłączono';showToast('Połączenie z pokojem zostało przerwane',true);setTimeout(showLobby,700);}};
}
document.querySelector('#soloMode').onclick=startSolo;document.querySelector('#createMode').onclick=()=>openRoomForm('create');document.querySelector('#joinMode').onclick=()=>openRoomForm('join');
document.querySelector('#refreshRooms').onclick=refreshRooms;
document.querySelector('#roomCancel').onclick=()=>{stopRoomRefresh();document.querySelector('#roomForm').classList.add('hidden');document.querySelector('.mode-grid').classList.remove('hidden');};
document.querySelector('#roomCode').addEventListener('input',e=>{e.target.value=e.target.value.toUpperCase().replace(/[^A-Z0-9]/g,'').slice(0,6);});
document.querySelector('#roomForm').onsubmit=async e=>{e.preventDefault();if(busy)return;const error=document.querySelector('#lobbyError');error.textContent='';const password=document.querySelector('#roomPassword').value;if(!password){error.textContent='Wpisz hasło pokoju.';document.querySelector('#roomPassword').focus();return;}let code=document.querySelector('#roomCode').value.trim().toUpperCase();if(roomAction==='create'){setBusy(true,'TWORZENIE…');try{const r=await fetch(`/api/room/create?password=${encodeURIComponent(password)}`,{method:'POST'});if(!r.ok)throw new Error();code=(await r.json()).room;}catch{setBusy(false);error.textContent='Nie udało się utworzyć pokoju.';showToast(error.textContent,true);return;}}if(code.length!==6){setBusy(false);error.textContent='Kod pokoju ma 6 znaków.';document.querySelector('#roomCode').focus();return;}setBusy(true,'ŁĄCZENIE…');connectRoom(code,password);};
document.querySelector('#newGame').onclick=async()=>{autoplayToken++;flipped=botSelect.value==='white';state=await fetch(`/api/new?bot=${botSelect.value}&time=${timeSelect.value}&${sessionQuery}`,{method:'POST'}).then(r=>r.json());selected=null;render();if(botSelect.value==='both')runAutoplay();};document.querySelector('#flipBoard').onclick=()=>{flipped=!flipped;selected=null;render();};document.querySelector('#exitGame').onclick=showLobby;
roomBadge.onclick=async()=>{const code=roomBadge.dataset.code;if(!code)return;try{await navigator.clipboard.writeText(code);showToast(`Skopiowano kod pokoju: ${code}`);}catch{showToast(`Kod pokoju: ${code}`);}};
document.addEventListener('keydown',event=>{if(event.key!=='Escape')return;if(selected!==null){selected=null;render();return;}if(!lobby.classList.contains('hidden')&&!document.querySelector('#roomForm').classList.contains('hidden'))document.querySelector('#roomCancel').click();});
document.body.classList.add('lobby-open');load();
