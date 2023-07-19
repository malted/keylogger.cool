use tungstenite::{connect, WebSocket, Message, stream::MaybeTlsStream, handshake::client::Response};
use std::net::TcpStream;
use serde_json::json;
use parking_lot::Mutex;
use once_cell::sync::Lazy;

#[repr(C)]
#[derive(Debug)]
pub struct ActionEvent {
    pub timestamp_start: i64, // UNIX s
    pub time_down: i64, // ms
    pub key_code: i16,
}

static mut WS_CLIENT: Lazy<Mutex<(WebSocket<MaybeTlsStream<TcpStream>>, Response)>> = Lazy::new(|| {
    let (socket, response) = connect("wss://wskc.malted.repl.co").expect("Can't connect");
    Mutex::new((socket, response))
});

#[no_mangle]
pub unsafe extern "C" fn action_event_delegate(c_struct: *mut ActionEvent) {
    let ae = &*c_struct;

	let res = WS_CLIENT.lock().0.write_message(Message::Text(json!({
		"start": ae.timestamp_start,
		"down": ae.time_down,
		"code": ae.key_code
	}).to_string()));

	if res.is_err() {
		println!("Reconnecting...");
		WS_CLIENT = Lazy::new(|| {
			let (socket, response) = connect("wss://wskc.malted.repl.co").expect("Can't connect");
			Mutex::new((socket, response))
		});
		println!("Reconnected!");
	}

	println!("{:?}", res);
}
