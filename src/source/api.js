	//ESP32S2 usb functions -

	function USBWait() {
		var xhr = new XMLHttpRequest();
		xhr.onreadystatechange = function() {
			if(xhr.readyState == XMLHttpRequest.DONE) {
				wait = xhr.response;
			}
		}
		xhr.open('POST', '/usbwait', false);
		xhr.send(null);
		return wait;
	}

	function disableUSB() {
		var getpl = new XMLHttpRequest();
		getpl.open("POST", "./usboff", true);
		getpl.send(null);
	}

	function enableUSB() {
		var getpl = new XMLHttpRequest();
		getpl.open("POST", "./usbon", true);
		getpl.send(null);
	}

	function ShutDown() {
		var getpl = new XMLHttpRequest();
		getpl.open("POST", "./sleep", true);
		getpl.send(null);
	}

	function tempc() {
		var xhr1 = new XMLHttpRequest();
		xhr1.onreadystatechange = function() {
			if(xhr1.readyState == XMLHttpRequest.DONE) {
				temp = xhr1.response;
			}
		}
		xhr1.open('POST', '/tempc', false);
		xhr1.send(null);
		return temp;
	}

	function fanthres() {
		var xhr2 = new XMLHttpRequest();
		xhr2.onreadystatechange = function() {
			if(xhr2.readyState == XMLHttpRequest.DONE) {
				fanth = xhr2.response;
			}
		}
		xhr2.open('POST', '/fan', false);
		xhr2.send(null);
		return fanth;
	}
