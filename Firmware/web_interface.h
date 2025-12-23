// File: web_interface.h

// --- TRANG ADMIN (DASHBOARD) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ADMIN DASHBOARD</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f4; }
    table { width: 90%; margin: 20px auto; border-collapse: collapse; background: white; }
    th, td { padding: 12px; border-bottom: 1px solid #ddd; text-align: center; }
    th { background-color: #007BFF; color: white; }
    .btn { background-color: #28a745; color: white; padding: 10px 20px; border: none; margin: 10px; border-radius: 5px; cursor: pointer;}
  </style>
</head>
<body>
  <h2>QUAN LY HANG DOI (ADMIN)</h2>
  <button class="btn" onclick="exportTableToCSV('log_hang_doi.csv')">XUAT EXCEL</button>
  <div style="margin: 10px;">
    <a href="/khach" target="_blank" style="color: blue;">Mo Trang Khach Hang >></a>
  </div>
  <table id="dataTable">
    <thead>
      <tr><th>Ticket #</th><th>Time Served</th></tr>
    </thead>
    <tbody></tbody>
  </table>
<script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  function initWebSocket() {
    websocket = new WebSocket(gateway);
    websocket.onopen = function() { console.log('Connected'); };
    websocket.onclose = function() { setTimeout(initWebSocket, 2000); };
    websocket.onmessage = function(event) {
      var data = JSON.parse(event.data);
      
      // [CODE MOI] Nhan lenh Reset tu ESP32
      if (data.type == "RESET") {
         var tableBody = document.getElementById("dataTable").getElementsByTagName('tbody')[0];
         tableBody.innerHTML = ""; // Xoa trang bang
         return;
      }

      var table = document.getElementById("dataTable").getElementsByTagName('tbody')[0];
      var newRow = table.insertRow(0);
      newRow.insertCell(0).innerHTML = "#" + data.ticket;
      newRow.insertCell(1).innerHTML = data.time;
    };
  }
  window.addEventListener('load', initWebSocket);

  function exportTableToCSV(filename) {
    var csv = [];
    var rows = document.querySelectorAll("table tr");
    for (var i = 0; i < rows.length; i++) {
        var row = [], cols = rows[i].querySelectorAll("td, th");
        for (var j = 0; j < cols.length; j++) row.push(cols[j].innerText);
        csv.push(row.join(","));        
    }
    var csvFile = new Blob([csv.join("\n")], {type: "text/csv"});
    var link = document.createElement("a");
    link.download = filename;
    link.href = window.URL.createObjectURL(csvFile);
    link.style.display = "none";
    document.body.appendChild(link);
    link.click();
  }
</script>
</body>
</html>
)rawliteral";

// --- TRANG KHÁCH HÀNG (CLIENT CHECK) ---
const char client_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>TRA CUU THOI GIAN CHO</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Segoe UI', Arial, sans-serif; text-align: center; background: #e0f7fa; padding: 20px; }
    .card { background: white; padding: 30px; border-radius: 15px; box-shadow: 0 4px 8px rgba(0,0,0,0.2); max-width: 400px; margin: auto; }
    input { padding: 10px; font-size: 18px; width: 80%; text-align: center; border: 2px solid #007BFF; border-radius: 5px; margin-bottom: 20px;}
    .status { font-size: 1.2em; color: #555; margin-top: 20px; }
    .highlight { font-size: 2em; color: #d32f2f; font-weight: bold; display: block; margin: 10px 0; }
    .current-serve { background: #007BFF; color: white; padding: 10px; border-radius: 8px; margin-bottom: 20px; }
  </style>
</head>
<body>
  <div class="card">
    <div class="current-serve">
      <h3>DANG PHUC VU SO:</h3>
      <h1 id="currentTicket">--</h1>
    </div>

    <label>Nhap so thu tu cua ban:</label><br>
    <input type="number" id="myTicket" placeholder="Vi du: 15" oninput="calculateWait()">

    <div id="resultBox" class="status">
      Nhap so de xem thoi gian cho...
    </div>
  </div>

<script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  var currentProcessing = 0;
  var avgTime = 30; 

  function initWebSocket() {
    websocket = new WebSocket(gateway);
    websocket.onopen = function() { console.log('Client Connected'); };
    websocket.onclose = function() { setTimeout(initWebSocket, 2000); };
    websocket.onmessage = function(event) {
      var data = JSON.parse(event.data);

      // [CODE MOI] Nhan lenh Reset
      if (data.type == "RESET") {
         currentProcessing = 0;
         document.getElementById("currentTicket").innerHTML = "--";
         document.getElementById("resultBox").innerHTML = "He thong vua Reset!";
         return;
      }

      currentProcessing = parseInt(data.ticket);
      if(data.avg) avgTime = parseFloat(data.avg);

      document.getElementById("currentTicket").innerHTML = "#" + currentProcessing;
      calculateWait();
    };
  }

  function calculateWait() {
    var myTicket = parseInt(document.getElementById("myTicket").value);
    var box = document.getElementById("resultBox");

    if (!myTicket) {
      box.innerHTML = "Vui long nhap so cua ban.";
      return;
    }

    if (myTicket <= currentProcessing) {
      box.innerHTML = "<span style='color:green; font-weight:bold'>DA DEN LUOT BAN HOAC DA QUA!</span><br>Vui long den quay ngay.";
    } else {
      var peopleAhead = myTicket - currentProcessing;
      var waitSeconds = peopleAhead * avgTime;
      
      var minutes = Math.floor(waitSeconds / 60);
      var seconds = Math.floor(waitSeconds % 60);

      box.innerHTML = "Con " + peopleAhead + " nguoi nua.<br>" +
                      "Thoi gian cho du kien:<br>" +
                      "<span class='highlight'>" + minutes + " phut " + seconds + " giay</span>";
    }
  }

  window.addEventListener('load', initWebSocket);
</script>
</body>
</html>
)rawliteral";