#include <opencv2/opencv.hpp>
#include <opencv2/face.hpp>
#include <lgpio.h>

#include <iostream>
#include <vector>
#include <deque>
#include <numeric>
#include <unistd.h>
#include <cstring>
#include <csignal>

#include <chrono>
#include <fstream>
#include <ctime>

// GPIOs usados 
const int PIN_BUZZER = 18;           
const int PIN_LED_SONOLENCIA = 17;   
const int PIN_LED_BOCEJO = 27; 
const int PIN_LED_FALHA = 22; 

int h_gpio;

// Controle de encerramento limpo via sinal (SIGINT/SIGTERM) ----
// systemd manda SIGTERM ao dar "systemctl stop"; sem isso o processo
// pode ser morto no meio de uma escrita de GPIO/CSV.
volatile sig_atomic_t g_running = 1;

void signal_handler(int signum) {
    g_running = 0;
}

// EAR dos olhos
double eye_aspect_ratio(const std::vector<cv::Point2f>& eye) {
    double A = cv::norm(eye[1] - eye[5]);
    double B = cv::norm(eye[2] - eye[4]);
    double C = cv::norm(eye[0] - eye[3]);
    return (A + B) / (2.0 * C);
}

// MAR da boca
double mouth_aspect_ratio(const std::vector<cv::Point2f>& mouth) {
    double A = cv::norm(mouth[1] - mouth[7]);
    double B = cv::norm(mouth[2] - mouth[6]);
    double C = cv::norm(mouth[3] - mouth[5]);
    double D = cv::norm(mouth[0] - mouth[4]);
    return (A + B + C) / (3.0 * D);
}

// HDR da Cabeça (Head Drop Ratio)
double head_drop_ratio(const std::vector<cv::Point2f>& shape) {
    double dist_nose_to_chin = cv::norm(shape[30] - shape[8]);
    double dist_eyes_to_nose = cv::norm(shape[27] - shape[30]);
    
    // Evita divisao por zero
    if(dist_eyes_to_nose == 0) return 0;
    
    return dist_nose_to_chin / dist_eyes_to_nose;
}

// Verifica brilho dos olhos para detectar Oculos Escuros
double get_eye_brightness(const cv::Mat& gray, const std::vector<cv::Point2f>& eye) {
    cv::Rect eye_rect = cv::boundingRect(eye);
    // Garante que o retangulo nao ultrapassa a imagem
    eye_rect &= cv::Rect(0, 0, gray.cols, gray.rows); 
    
    if (eye_rect.area() > 0) {
        cv::Scalar mean_brightness = cv::mean(gray(eye_rect));
        return mean_brightness[0];
    }
    return 255.0; 
}


// Salvar eventos CSV

void salvar_evento(std::string tipo, double valor, int duracao) {
    std::ofstream arquivo("eventos_sonolencia.csv", std::ios::app);

    if(!arquivo.is_open()) {
        std::cerr << "Erro ao abrir CSV" << std::endl;
        return;
    }

    time_t agora = time(nullptr);
    tm* tempo = localtime(&agora);

    arquivo
    << tempo->tm_mday << "/"
    << tempo->tm_mon + 1 << "/"
    << tempo->tm_year + 1900 << ","
    << tempo->tm_hour << ":"
    << tempo->tm_min << ":"
    << tempo->tm_sec << ","
    << tipo << ","
    << valor << ","
    << duracao << "\n";

    arquivo.close();
    std::cout << "Evento salvo: " << tipo << std::endl;
}


int main(int argc, char** argv) {

    //modo GUI opcional 
    // Uso: ./detector_atencao        -> headless (producao, sem monitor)
    //      ./detector_atencao --gui  -> abre janela (bancada/debug com HDMI)
    bool debug_gui = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--gui") == 0) {
            debug_gui = true;
        }
    }

    //registra handlers de sinal para encerramento limpo ----
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    h_gpio = lgGpiochipOpen(0);

    if(h_gpio < 0) {
        std::cerr << "Falha GPIO" << std::endl;
        return -1;
    }

    // Configurando os pinos como saida
    // fecha o handle do GPIO antes de sair em caso de erro 
    if(lgGpioClaimOutput(h_gpio, 0, PIN_LED_SONOLENCIA, 0) < 0) { lgGpiochipClose(h_gpio); return -1; }
    if(lgGpioClaimOutput(h_gpio, 0, PIN_LED_BOCEJO, 0) < 0) { lgGpiochipClose(h_gpio); return -1; }
    if(lgGpioClaimOutput(h_gpio, 0, PIN_BUZZER, 0) < 0) { lgGpiochipClose(h_gpio); return -1; }
    if(lgGpioClaimOutput(h_gpio, 0, PIN_LED_FALHA, 0) < 0) { lgGpiochipClose(h_gpio); return -1; }

    // Desligando tudo inicialmente
    lgGpioWrite(h_gpio, PIN_LED_SONOLENCIA, 0);
    lgGpioWrite(h_gpio, PIN_LED_BOCEJO, 0);
    lgGpioWrite(h_gpio, PIN_BUZZER, 0);
    lgGpioWrite(h_gpio, PIN_LED_FALHA, 0);

    // Aqui é o cabecalho do CSV 
    std::ofstream arquivo("eventos_sonolencia.csv", std::ios::app);
    if(arquivo.tellp() == 0) {
        arquivo << "data,hora,evento,valor,duracao\n";
    }
    arquivo.close();

    // Calibracao e ajuste de variaveis
    bool is_calibrating = true;
    int calibration_frames = 0;
    const int CALIBRATION_MAX = 60;

    double sum_calibration_ear = 0.0;
    double sum_calibration_mar = 0.0;
    double sum_calibration_hdr = 0.0;

    double EAR_THRESHOLD = 0.0;
    double MAR_THRESHOLD = 0.0;
    double HDR_THRESHOLD = 0.0;

    // Fatores de Adaptação que irao funcionar como coeficientes para as medias
    const double DROP_FACTOR = 0.75;  // Multiplicador para queda do olho
    const double RISE_FACTOR = 1.50;  // Multiplicador para expansão da boca (bocejo)
    const double HDR_DROP_FACTOR = 0.75; // Multiplicador para queda da cabeca

    const int CONSECUTIVE_FRAMES = 20;
    int frame_counter = 0;
    
    // Filtro de Média Móvel (Tamanho da Janela)
    const int WINDOW_SIZE = 5;
    std::deque<double> ear_history;
    std::deque<double> mar_history;
    std::deque<double> hdr_history;
    
    int yawn_counter = 0;
    int head_counter = 0;
    const int YAWN_CONSECUTIVE_FRAMES = 25; 
    const int HEAD_CONSECUTIVE_FRAMES = 15; 
    
    bool sleep_saved = false;
    bool yawn_saved = false;
    bool head_saved = false;

    // Variaveis de Falha
    int frames_sem_rosto = 0;
    int frames_oculos_escuro = 0;
    bool block_alarms_due_to_sunglasses = false;

    // Carregamento dos modelos
    cv::CascadeClassifier face_detector;
    if(!face_detector.load("haarcascade_frontalface_alt2.xml")) {
        std::cerr << "Erro ao carregar Haar Cascade" << std::endl;
        lgGpioWrite(h_gpio, PIN_LED_FALHA, 1); // Falha de inicializacao
        usleep(300000); // ---- CORRIGIDO: da tempo do LED acender antes de fechar o handle
        lgGpiochipClose(h_gpio); // ---- CORRIGIDO: fecha o handle antes de sair
        return -1;
    }

    cv::Ptr<cv::face::Facemark> facemark = cv::face::createFacemarkLBF();
    try {
        facemark->loadModel("lbfmodel.yaml");
    } catch(const cv::Exception& e) {
        std::cerr << "Erro modelo LBF: " << e.what() << std::endl;
        // ---- CORRIGIDO: escreve no LED ANTES de fechar o handle (ordem original estava invertida,
        // fazendo o lgGpioWrite tentar escrever num handle ja fechado)
        lgGpioWrite(h_gpio, PIN_LED_FALHA, 1); // Outra falha de inicializacao
        usleep(300000);
        lgGpiochipClose(h_gpio);
        return -1;
    }

    // usa symlink fixo criado via regra udev, em vez de indice
    // numerico (/dev/videoN), que pode mudar a cada boot dependendo da ordem
    // de enumeracao USB. Ver /etc/udev/rules.d/99-camera-atencao.rules
    cv::VideoCapture cap("/dev/camera_atencao", cv::CAP_V4L2);
    if(!cap.isOpened()) {
        std::cerr << "Erro ao abrir camera" << std::endl;
        lgGpioWrite(h_gpio, PIN_LED_FALHA, 1);
        usleep(300000); // ---- CORRIGIDO: da tempo do LED acender antes de fechar o handle
        lgGpiochipClose(h_gpio); // ---- CORRIGIDO: fecha o handle antes de sair
        return -1;
    }

    cv::Mat frame, gray;
    std::cout << "Monitor iniciado... (modo " << (debug_gui ? "GUI" : "headless") << ")" << std::endl;

    while(g_running) {
        cap >> frame;
        
        // Falha Runtime de Camera
        if(frame.empty()) {
            std::cerr << "Falha: Câmera desconectada inesperadamente" << std::endl;
            lgGpioWrite(h_gpio, PIN_LED_FALHA, 1); 
            break;
        }

        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::equalizeHist(gray, gray);

        std::vector<cv::Rect> faces;
        face_detector.detectMultiScale(gray, faces, 1.1, 4, 0, cv::Size(100,100));

        if(faces.empty()) {
            // Falha de Sistema Cego
            frames_sem_rosto++;
            if(frames_sem_rosto > 90) { // Aproximadamente 3 segundos a 30fps
                lgGpioWrite(h_gpio, PIN_LED_FALHA, 1);
                if (debug_gui) { // ---- CORRIGIDO: putText so roda se a janela vai ser exibida
                    cv::putText(frame, "SISTEMA CEGO / SEM ROSTO", cv::Point(30, 100),
                                cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2);
                }
            }

            if(!is_calibrating) {
                frame_counter = 0;
                yawn_counter = 0;
                head_counter = 0;
                
                sleep_saved = false;
                yawn_saved = false;
                head_saved = false;

                lgGpioWrite(h_gpio, PIN_BUZZER, 0);
                lgGpioWrite(h_gpio, PIN_LED_SONOLENCIA, 0);
                lgGpioWrite(h_gpio, PIN_LED_BOCEJO, 0);
            }
        } else {
            frames_sem_rosto = 0; // Reseta falha de rosto ausente

            std::vector<std::vector<cv::Point2f>> landmarks;
            bool success = facemark->fit(frame, faces, landmarks);

            if(success) {
                std::vector<cv::Point2f>& shape = landmarks[0];
                std::vector<cv::Point2f> left_eye, right_eye, mouth;

                for(int i=36; i<=41; i++) left_eye.push_back(shape[i]);
                for(int i=42; i<=47; i++) right_eye.push_back(shape[i]);
                for(int i=60; i<=67; i++) mouth.push_back(shape[i]); 

                // Logica de Oculos Escuros
                double left_bright = get_eye_brightness(gray, left_eye);
                double right_bright = get_eye_brightness(gray, right_eye);
                double avg_bright = (left_bright + right_bright) / 2.0;

                if (avg_bright < 35.0) {
                    frames_oculos_escuro++;
                } else {
                    frames_oculos_escuro = 0;
                }

                if (frames_oculos_escuro > 30) {
                    lgGpioWrite(h_gpio, PIN_LED_FALHA, 1);
                    block_alarms_due_to_sunglasses = true;
                    if (debug_gui) { // ---- CORRIGIDO: putText so roda se a janela vai ser exibida
                        cv::putText(frame, "FALHA: OCULOS ESCUROS DETECTADO", cv::Point(30, 280),
                                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 255), 2);
                    }
                } else {
                    block_alarms_due_to_sunglasses = false;
                    if (frames_sem_rosto == 0) {
                        lgGpioWrite(h_gpio, PIN_LED_FALHA, 0); // Apaga o LED de falha, caso volte ao normal
                    }
                }

                // Valores RAW
                double ear_raw = (eye_aspect_ratio(left_eye) + eye_aspect_ratio(right_eye)) / 2.0;
                double mar_raw = mouth_aspect_ratio(mouth);
                double hdr_raw = head_drop_ratio(shape);

                // Aplica Média Móvel EAR
                ear_history.push_back(ear_raw);
                if(ear_history.size() > WINDOW_SIZE) ear_history.pop_front();
                double ear_smooth = std::accumulate(ear_history.begin(), ear_history.end(), 0.0) / ear_history.size();

                // Aplica Média Móvel MAR 
                mar_history.push_back(mar_raw);
                if(mar_history.size() > WINDOW_SIZE) mar_history.pop_front();
                double mar_smooth = std::accumulate(mar_history.begin(), mar_history.end(), 0.0) / mar_history.size();

                // Aplica Média Móvel HDR (Cabeca)
                hdr_history.push_back(hdr_raw);
                if(hdr_history.size() > WINDOW_SIZE) hdr_history.pop_front();
                double hdr_smooth = std::accumulate(hdr_history.begin(), hdr_history.end(), 0.0) / hdr_history.size();

                // Rotina de calibracao
                if(is_calibrating) {
                    sum_calibration_ear += ear_smooth;
                    sum_calibration_mar += mar_smooth; 
                    sum_calibration_hdr += hdr_smooth;
                    
                    calibration_frames++;

                    if (debug_gui) { // ---- CORRIGIDO: putText so roda se a janela vai ser exibida
                        cv::putText(frame, "CALIBRANDO: OLHE PARA FRENTE", cv::Point(20,50),
                                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0,255,255), 2);
                    }

                    if(calibration_frames >= CALIBRATION_MAX) {
                        double avgEAR = sum_calibration_ear / CALIBRATION_MAX;
                        double avgMAR = sum_calibration_mar / CALIBRATION_MAX;
                        double avgHDR = sum_calibration_hdr / CALIBRATION_MAX;

                        EAR_THRESHOLD = avgEAR * DROP_FACTOR;
                        MAR_THRESHOLD = avgMAR * RISE_FACTOR;
                        HDR_THRESHOLD = avgHDR * HDR_DROP_FACTOR;
                        
                        is_calibrating = false;

                        std::cout << "Calibracao finalizada\n"
                                  << "EAR limite: " << EAR_THRESHOLD << "\n"
                                  << "MAR limite: " << MAR_THRESHOLD << "\n"
                                  << "HDR limite: " << HDR_THRESHOLD << std::endl;

                        lgGpioWrite(h_gpio, PIN_BUZZER, 1);
                        usleep(200000);
                        lgGpioWrite(h_gpio, PIN_BUZZER, 0);
                    }
                }
                else {
                    bool eye_alert = false;
                    bool yawn_alert = false;
                    bool head_alert = false;

                    // LOGICA DO OLHO
                    if(!block_alarms_due_to_sunglasses && ear_smooth < EAR_THRESHOLD) {
                        frame_counter++;
                        if(frame_counter >= CONSECUTIVE_FRAMES) {
                            eye_alert = true;
                            if(!sleep_saved) {
                                salvar_evento("SONOLENCIA", ear_smooth, frame_counter);
                                sleep_saved = true;
                            }
                            if (debug_gui) { // ---- CORRIGIDO: putText so roda se a janela vai ser exibida
                                cv::putText(frame, "ALERTA: MOTORISTA DORMINDO!", cv::Point(30, 150),
                                            cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2);
                            }
                        }
                    } else {
                        frame_counter = 0;
                        sleep_saved = false;
                    }

                    // LOGICA DO BOCEJO 
                    if(mar_smooth > MAR_THRESHOLD) {
                        yawn_counter++;
                        if(yawn_counter >= YAWN_CONSECUTIVE_FRAMES) {
                            yawn_alert = true;
                            if(!yawn_saved) {
                                salvar_evento("BOCEJO", mar_smooth, yawn_counter);
                                yawn_saved = true;
                            }
                            if (debug_gui) { // ---- CORRIGIDO: putText so roda se a janela vai ser exibida
                                cv::putText(frame, "ALERTA: BOCEJO!", cv::Point(30, 200),
                                            cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 165, 255), 2);
                            }
                        }
                    } else {
                        yawn_counter = 0;
                        yawn_saved = false;
                    }

                    // LOGICA DA CABEÇA BAIXA 
                    if(hdr_smooth < HDR_THRESHOLD) {
                        head_counter++;
                        if(head_counter >= HEAD_CONSECUTIVE_FRAMES) {
                            head_alert = true;
                            if(!head_saved) {
                                salvar_evento("CABECA_BAIXA", hdr_smooth, head_counter);
                                head_saved = true;
                            }
                            if (debug_gui) { // ---- CORRIGIDO: putText so roda se a janela vai ser exibida
                                cv::putText(frame, "ALERTA: CABECA BAIXA!", cv::Point(30, 250),
                                            cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 100, 255), 2);
                            }
                        }
                    } else {
                        head_counter = 0;
                        head_saved = false;
                    }

                    // SAÍDA HARDWARE (LEDs e Buzzer)                    
                    // O LED de Sonolência acende tanto para olho fechado quanto para cabeca baixa
                    lgGpioWrite(h_gpio, PIN_LED_SONOLENCIA, (eye_alert || head_alert) ? 1 : 0);
                    lgGpioWrite(h_gpio, PIN_LED_BOCEJO, yawn_alert ? 1 : 0);

                    // Controle de frequencia do buzzer por prioridade
                    int beep_interval = 0;
                    if (eye_alert) {
                        beep_interval = 150; // Sonolencia (Olhos): Bipe muito acelerado
                    } else if (head_alert) {
                        beep_interval = 300; // Cabeca Baixa: Bipe intermediario
                    } else if (yawn_alert) {
                        beep_interval = 600; // Bocejo: Bipe longo e espaçado
                    }

                    static bool buzzer_state = false;
                    static auto last_toggle = std::chrono::steady_clock::now();
                    
                    if (beep_interval > 0) {
                        auto now = std::chrono::steady_clock::now();
                        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_toggle).count() >= beep_interval) {
                            buzzer_state = !buzzer_state;
                            lgGpioWrite(h_gpio, PIN_BUZZER, buzzer_state ? 1 : 0);
                            last_toggle = now;
                        }
                    } else {
                        buzzer_state = false;
                        lgGpioWrite(h_gpio, PIN_BUZZER, 0); 
                    }

                    // Impressao dos valores na tela (HUD) -- so faz sentido em modo GUI
                    if (debug_gui) {
                        cv::putText(frame, "EAR: " + std::to_string(ear_smooth), cv::Point(10,30),
                                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255,255,0), 2);
                        cv::putText(frame, "MAR: " + std::to_string(mar_smooth), cv::Point(10,60),
                                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255,255,0), 2);
                        cv::putText(frame, "HDR: " + std::to_string(hdr_smooth), cv::Point(10,90),
                                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255,255,0), 2);
                        cv::putText(frame, "LUZ: " + std::to_string(avg_bright), cv::Point(10,120),
                                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255,150,0), 2);
                    }
                }

                // Desenhos na interface -- so fazem sentido em modo GUI (custo de CPU a toa em headless)
                if (debug_gui) {
                    for(int i=0; i<6; i++) {
                        cv::line(frame, left_eye[i], left_eye[(i+1)%6], cv::Scalar(0,255,0), 1);
                        cv::line(frame, right_eye[i], right_eye[(i+1)%6], cv::Scalar(0,255,0), 1);
                    }
                    for(int i=0; i<8; i++) {
                        cv::line(frame, mouth[i], mouth[(i+1)%8], cv::Scalar(255,0,255), 1);
                    }

                    // Linha Eixo do Nariz (Ponto 27 ao 30) - Azul
                    cv::line(frame, shape[27], shape[30], cv::Scalar(255,0,0), 2);
                    // Linha Queixo (Ponto 30 ao 8) - Vermelho
                    cv::line(frame, shape[30], shape[8], cv::Scalar(0,0,255), 2);
                }
            }
        }

        // ---- NOVO: imshow/waitKey só rodam em modo GUI ----
        if (debug_gui) {
            cv::imshow("Monitor de Atencao", frame);
            if(cv::waitKey(1) == 'q') break;
        }
    }

    std::cout << "Encerrando monitor..." << std::endl;

    lgGpioWrite(h_gpio, PIN_LED_SONOLENCIA, 0);
    lgGpioWrite(h_gpio, PIN_LED_BOCEJO, 0);
    lgGpioWrite(h_gpio, PIN_BUZZER, 0);
    lgGpioWrite(h_gpio, PIN_LED_FALHA, 0); // Apaga o LED de falha na saida
    lgGpiochipClose(h_gpio);

    cap.release();
    if (debug_gui) {
        cv::destroyAllWindows();
    }
    return 0;
}
