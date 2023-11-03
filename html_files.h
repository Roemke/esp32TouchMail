//die haupseite, sie wird im flash abgelegt (wegen PROGMEM) R" heißt "roh", rawliteral( als trennzeichen
//mal uebernommen
const char setup_html[] PROGMEM = R"rawliteral(<!doctype html> 
<!DOCTYPE html>
<html>
<head>
  <title>ESP Wi-Fi Manager</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="utf-8">
  <link rel="icon" href="data:,">
  <style>
    html {
      font-family: Arial, Helvetica, sans-serif; 
      display: inline-block; 
      text-align: center;
    }

    h1 {
      font-size: 1.8rem; 
      color: white;
    }

    p { 
      font-size: 1.4rem;
    }

    .topnav { 
      overflow: hidden; 
      background-color: #0A1128;
    }

    body {  
      margin: 0;
    }

    .content { 
      padding: 5%%;
    }

    .card-grid { 
      max-width: 800px; 
      margin: 0 auto; 
      display: grid; 
      grid-gap: 2rem; 
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    }

    .card { 
      background-color: white; 
      box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
    }

    .card-title { 
      font-size: 1.2rem;
      font-weight: bold;
      color: #034078
    }

    input[type=text], input[type=number], select {
      width: 50%%;
      padding: 12px 20px;
      margin: 18px;
      display: inline-block;
      border: 1px solid #ccc;
      border-radius: 4px;
      box-sizing: border-box;
    }

    label {
      font-size: 1.2rem; 
    }
    .value{
      font-size: 1.2rem;
      color: #1282A2;  
    }
    .state {
      font-size: 1.2rem;
      color: #1282A2;
    }
    button {
      border: none;
      color: #0F1163;
      padding: 15px 32px;
      text-align: center;
      font-size: 16px;
      border-radius: 4px;
      transition-duration: 0.4s;
      margin-top: 1em;
    }
    .button-on {
      background-color: #034078;
    }
    .button-on:hover {
      background-color: #1282A2;
    }
    .button-off {
      background-color: #858585;
    }
    .button-off:hover {
      background-color: #252524;
    }
    #info {
      padding-top: 1em;
    }   
  </style>
  <script>
    window.addEventListener("load", () => 
    {
      let resetB = document.getElementById("resetWifi");
      let testMailB = document.getElementById("testMail");
      let restartB = document.getElementById("restart");
      resetB.addEventListener("click", () => 
      {
        let url = '%RESETURL%';
        fetch(url) 
          .then(response=>response.json())
          .then(response => {//then above just returns response.json, this is a normal function body
            console.log("get response from server: " + response); 
            document.getElementById('headline').innerHTML = "Reset Wifi";
            document.getElementById('content').innerHTML = "You have to connect to %APNET% and reconfig on probably %APIP%" ;
          });
      });
      testMailB.addEventListener("click", () => 
      {
        document.getElementById('info').innerHTML = "Testmail send triggered";
        let url = '%TESTMAILURL%';
        fetch(url) 
          .then(response=>response.json())
          .then(response => {//then above just returns response.json, this is a normal function body
            console.log("get response from server: " + response); 
            document.getElementById('info').innerHTML = "Testmail should be send";
          });
      });
      restartB.addEventListener("click", () => 
      {
        let url = '%RESTARTURL%';
        fetch(url) 
          .then(response=>response.json())
          .then(response => {//then above just returns response.json, this is a normal function body
            console.log("get response from server: " + response); 
            document.getElementById('headline').innerHTML = "Restart";
            document.getElementById('info').innerHTML = "Restart of ESP triggered" ;
          });
      });
    });
  </script>
</head>
<body>
  <div class="topnav">
    <h1 id="headline">ESP Wi-Fi Manager</h1>
  </div>
  <div class="content" id="content">
    <div class="card-grid">
      <div class="card">
        <div id="info">
          Changing Network-Settings: You need to restart ESP, don't forget to save before :-)
        </div>
        <form action="/" method="POST">
          <p> <!-- hmm beim naechsten Projekt in einer Schleife generieren -->
            <label for="ssid">ESSID</label>
            <input type="text" id ="essid" name="essid" value="%ESSID%"><br>
            <label for="pass">Password (WLan)</label>
            <input type="text" id ="pass" name="pass" value="%PASS%"><br>
            <label for="httpdUser">Httpd-User (for this page)</label>
            <input type="text" id ="httpdUser" name="httpdUser" value="%HTTPDUSER%"><br>
            <label for="httpdPass">Password (for user)</label>
            <input type="text" id ="httpdPass" name="httpdPass" value="%HTTPDPASS%"><br>
            <label for="sender">E-Mail Sender</label>
            <input type="text" id ="sender" name="sender" value="%SENDER%"><br>
            <label for="smtpHost">SMTP-Host</label>
            <input type="text" id ="smtpHost" name="smtpHost" value="%SMTPHOST%"><br>
            <label for="smtpPort">SMTP-Port</label>
            <input type="text" id ="smtpPort" name="smtpPort" value="%SMTPPORT%"><br>
            <label for="appPass">App Pass (Google)</label>
            <input type="text" id ="appPass" name="appPass" value="%APPPASS%"><br>
            <label for="receiver">Send E-Mail to</label>
            <input type="text" id ="receiver" name="receiver" value="%RECEIVER%"><br>
            <label for="receiverName">Receiver Name</label>
            <input type="text" id ="receiverName" name="receiverName" value="%RECEIVERNAME%"><br>
            <label for="deepSleep">Deep Sleep after (seconds)</label>
            <input type="text" id ="deepSleep" name="deepSleep" value="%DEEPSLEEP%"><br>
            <label for="touchPin">Touch Pin to wake up (GPIO)</label>
            <input type="text" id ="touchPin" name="touchPin" value="%TOUCHPIN%"><br>            
            <label for="threshold">Threshold for touch</label>
            <input type="text" id ="threshold" name="threshold" value="%THRESHOLD%"><br>            
            <!-- nutze dhcp
              <label for="ip">IP Address</label>
              <input type="text" id ="ip" name="ip" value="192.168.1.200"><br>
              <label for="gateway">Gateway Address</label>
              <input type="text" id ="gateway" name="gateway" value="192.168.1.1"><br>
            -->
            <button type ="button" id="resetWifi">Reset WifiSettings</button>
            <button type ="button" id="testMail">TestMail</button>
            <button type ="button" id="restart">Restart</button>
            <button type ="submit" value ="Submit">Save</button>
          </p>
        </form>
      </div>
    </div>
    <p>MAC des Geräts: %MAC%</p>
  </div>
</body>
</html>
)rawliteral";
