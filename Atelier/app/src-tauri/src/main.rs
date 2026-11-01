// Atelier: a janela do app. Liga o serviço da fila (node Atelier/core/atelier.mjs) se ele não estiver
// rodando e abre a tela dele. O serviço continua de pé com a janela fechada, pras sessões pedirem imagem.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::net::{SocketAddr, TcpStream};
use std::os::windows::process::CommandExt;
use std::path::PathBuf;
use std::process::Command;
use std::thread;
use std::time::{Duration, Instant};
use tauri::webview::NewWindowResponse;
use tauri::{WebviewUrl, WebviewWindowBuilder};

const PORT: u16 = 8140;
const CREATE_NO_WINDOW: u32 = 0x0800_0000;
// o repositório do jogo; ATELIER_REPO troca, se um dia ele mudar de lugar
const REPO_DEFAULT: &str = r"C:\Users\luisf\explosion";

fn service_up() -> bool {
    let addr = SocketAddr::from(([127, 0, 0, 1], PORT));
    TcpStream::connect_timeout(&addr, Duration::from_millis(300)).is_ok()
}

fn start_service() {
    let repo = std::env::var("ATELIER_REPO").map(PathBuf::from).unwrap_or_else(|_| PathBuf::from(REPO_DEFAULT));
    let script = repo.join("Atelier").join("core").join("atelier.mjs");
    let _ = Command::new("node").arg(script).current_dir(&repo).creation_flags(CREATE_NO_WINDOW).spawn();
}

fn main() {
    tauri::Builder::default()
        .setup(|app| {
            let mut ok = service_up();
            if !ok {
                start_service();
                let t = Instant::now();
                while !ok && t.elapsed() < Duration::from_secs(12) {
                    thread::sleep(Duration::from_millis(200));
                    ok = service_up();
                }
            }
            let url = if ok {
                WebviewUrl::External(format!("http://127.0.0.1:{PORT}/").parse().unwrap())
            } else {
                WebviewUrl::App("index.html".into())
            };
            // sem o tratador de arrastar do Tauri: o soltar arquivo fica com a página (anexos)
            WebviewWindowBuilder::new(app, "main", url)
                .title("Atelier")
                .inner_size(1600.0, 960.0)
                .min_inner_size(900.0, 600.0)
                .center()
                .disable_drag_drop_handler()
                // link que abriria janela nova (ex.: criar chave no AI Studio) vai pro navegador padrão
                .on_new_window(|url, _| {
                    if matches!(url.scheme(), "http" | "https") {
                        let _ = Command::new("rundll32").args(["url.dll,FileProtocolHandler", url.as_str()]).creation_flags(CREATE_NO_WINDOW).spawn();
                    }
                    NewWindowResponse::Deny
                })
                .build()?;
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("não consegui abrir o Atelier");
}
