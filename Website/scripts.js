//const app = document.getElementById('demo');
//
//const logo = document.createElement('img');
//logo.src = 'logo.png';
//
//const container = document.createElement('div');
//container.setAttribute('class', 'container');
//
//app.appendChild(logo);
//app.appendChild(container);

function loadXMLDoc() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
            var obj = JSON.parse(this.responseText);
            document.getElementById("UtcTime").innerHTML = "UTC Time = "+obj.UtcTime;
            document.getElementById("EpochTime").innerHTML = "Epoch Time = "+obj.EpochTime;
            document.getElementById("ThermometerId").innerHTML = "Thermometer Id = "+obj.ThermometerId;
            document.getElementById("Temperature").innerHTML = "Temperature = "+obj.Temperature+"°C";
            document.getElementById("Hostname").innerHTML = "Hostname = "+obj.Hostname;
            document.getElementById("IpAddress").innerHTML = "IPAddress = "+obj.IpAddress;
            document.getElementById("Gpio").innerHTML = "GPIO="+obj.Gpio;
          }
        };
        xhttp.open("GET", "http://192.168.63.220/temps", true);
        xhttp.send();
      }
