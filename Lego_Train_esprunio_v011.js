var devices;
var gatt;
var connected = 0;
var light_on = 0;
var DevName="e0:7d:ea:0c:03:29";
var serviceUUID="00001623-1212-efde-1623-785feabcd123";
var characteristicUUID="00001624-1212-efde-1623-785feabcd123";
pinMode(D15, 'input_pulldown');
pinMode(D16, 'input_pulldown');
pinMode(D17, 'input_pulldown');
pinMode(D18, 'input_pulldown');
pinMode(D31, 'input_pulldown');
global.LED2=D2;

var Characteristic_store;

function connect_train() {
  if (connected == 0) {
    NRF.connect("e0:7d:ea:0c:03:29").then(function(g) {
      console.log("Starting connecting2");
      gatt = g;
      return gatt.getPrimaryService("00001623-1212-efde-1623-785feabcd123");
    }).then(function(service) {
      return service.getCharacteristic("00001624-1212-efde-1623-785feabcd123");
    }).then(function(characteristic) {
      Characteristic_store = characteristic;
        return characteristic.readValue();
      }).then(value => {
        console.log(value);
    }).then(function() {
      console.log("Train Connected");
      connected = 1;
  });
  } else {
    gatt.disconnect();
    console.log("Train Disconnected");
    connected = 0;
  }
}

function play_horn() {
  console.log("message = ", Characteristic_store);
  const prepval = new Uint8Array([0x0a, 0x00, 0x41, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01]);
  const sendvalue = new Uint8Array([0x08, 0x00, 0x81, 0x01, 0x11, 0x51, 0x01, 0x09]);

  Characteristic_store.writeValue(prepval)
  .then(_ => {
    Characteristic_store.writeValue(sendvalue);
  });
}

function light_state() {
  console.log("message = ", Characteristic_store);
  const prepval = new Uint8Array([0x0a, 0x00, 0x41, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01]);
  if (light_on == 0) {
    const sendvalue = new Uint8Array([0x08, 0x00, 0x81, 0x11, 0x11, 0x51, 0x00, 0x0a]);
    light_on = 1;
  } else {
    const sendvalue = new Uint8Array([0x08, 0x00, 0x81, 0x11, 0x11, 0x51, 0x00, 0x00]);
    light_on = 0;
  }
  Characteristic_store.writeValue(prepval)
  .then(_ => {
    Characteristic_store.writeValue(sendvalue);
  });
}

function fill_water() {
  console.log("message = ", Characteristic_store);
  const prepval = new Uint8Array([0x0a, 0x00, 0x41, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01]);
  const sendvalue = new Uint8Array([0x08, 0x00, 0x81, 0x01, 0x11, 0x51, 0x01, 0x07]);

  Characteristic_store.writeValue(prepval)
  .then(_ => {
    Characteristic_store.writeValue(sendvalue)
    .then(_ => {
      console.log(sendvalue);
    });
  });
}


function move_train() {
  console.log("message = ", Characteristic_store);
  const prepval = new Uint8Array([0x0a, 0x00, 0x41, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01]);
  const sendvalue = new Uint8Array([0x08, 0x00, 0x81, 0x00, 0x01, 0x51, 0x00, 0x64]);
  Characteristic_store.writeValue(prepval)
  .then(_ => {
    Characteristic_store.writeValue(sendvalue)
    .then(_ => {
      console.log(sendvalue);
    });
  });
}

function train_direction(dir_val) {
  //console.log("message = ", Characteristic_store);
  const prepval = new Uint8Array([0x0a, 0x00, 0x41, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01]);
  const sendvalue = new Uint8Array([0x08, 0x00, 0x81, 0x00, 0x01, 0x51, 0x00, 0x00]);
  if (dir_val > 300 && dir_val < 400) {
    const sendvalue = new Uint8Array([0x08, 0x00, 0x81, 0x00, 0x01, 0x51, 0x00, 0x1e]);
  }
  if (dir_val > 400 && dir_val < 500) {
    const sendvalue = new Uint8Array([0x08, 0x00, 0x81, 0x00, 0x01, 0x51, 0x00, 0x32]);
  }
  if (dir_val > 500) {
    const sendvalue = new Uint8Array([0x08, 0x00, 0x81, 0x00, 0x01, 0x51, 0x00, 0x64]);
  }
  if (dir_val < 300 && dir_val > 200) {
    const sendvalue = new Uint8Array([0x08, 0x00, 0x81, 0x00, 0x01, 0x51, 0x00, 0xe2]);
  }
  if (dir_val < 200 && dir_val > 100) {
    const sendvalue = new Uint8Array([0x08, 0x00, 0x81, 0x00, 0x01, 0x51, 0x00, 0xce]);
  }
  if (dir_val < 100) {
    const sendvalue = new Uint8Array([0x08, 0x00, 0x81, 0x00, 0x01, 0x51, 0x00, 0x9c]);
  }
  Characteristic_store.writeValue(prepval)
  .then(_ => {
    Characteristic_store.writeValue(sendvalue);
  });
}

connect_train();

setInterval(function() {
  digitalWrite(LED2, digitalRead(D15));
  var reading = analogRead(D31);
  //console.log(reading * 1024);
  if (connected == 1) {
    if (reading * 1024 > 300 || reading * 1024 < 200) {
      train_direction(reading * 1024);
    }
  }
  if (digitalRead(D15) == 1) {
    //connect_train();
    play_horn();
    console.log('D15');
  }
  if (digitalRead(D16) == 1) {
    //sound_message();
    fill_water();
    console.log('D16');
  }
  if (digitalRead(D17) == 1) {
    //connect_train();
    light_state();
    console.log('D17');
  }
  if (digitalRead(D18) == 1) {
    //connect_train();
    console.log('D18');
  }
}, 100);

