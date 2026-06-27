#include <opencv2/opencv.hpp>
#include <opencv2/face.hpp>
#include <lgpio.h>

#include <iostream>
#include <vector>
#include <deque>
#include <numeric>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <ctime>


// =======================================
// GPIO
// =======================================

const int PIN_BUZZER = 18;           
const int PIN_LED_SONOLENCIA = 17;   

int h_gpio;


// =======================================
// Funções de cálculo
// =======================================


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



// =======================================
// Salvar eventos CSV
// =======================================

void salvar_evento(std::string tipo, double valor, int duracao)
{

    std::ofstream arquivo(
        "eventos_sonolencia.csv",
        std::ios::app
    );


    if(!arquivo.is_open())
    {
        std::cerr << "Erro ao abrir CSV" << std::endl;
        return;
    }


    time_t agora = time(nullptr);

    tm* tempo = localtime(&agora);



    arquivo
    << tempo->tm_mday << "/"
    << tempo->tm_mon + 1 << "/"
    << tempo->tm_year + 1900
    << ","

    << tempo->tm_hour << ":"
    << tempo->tm_min << ":"
    << tempo->tm_sec

    << ","

    << tipo

    << ","

    << valor

    << ","

    << duracao

    << "\n";



    arquivo.close();


    std::cout 
    << "Evento salvo: "
    << tipo
    << std::endl;

}





// =======================================
// MAIN
// =======================================


int main() {


    h_gpio = lgGpiochipOpen(0);


    if(h_gpio < 0)
    {

        std::cerr 
        << "Falha GPIO"
        << std::endl;

        return -1;
    }



    int ret;



    ret = lgGpioClaimOutput(
        h_gpio,
        0,
        PIN_LED_SONOLENCIA,
        0
    );


    if(ret < 0)
    {
        return -1;
    }




    ret = lgGpioClaimOutput(
        h_gpio,
        0,
        PIN_BUZZER,
        0
    );


    if(ret < 0)
    {
        return -1;
    }




    lgGpioWrite(
        h_gpio,
        PIN_LED_SONOLENCIA,
        0
    );


    lgGpioWrite(
        h_gpio,
        PIN_BUZZER,
        0
    );



    // ==================================
    // CSV cabeçalho
    // ==================================

    std::ofstream arquivo(
        "eventos_sonolencia.csv",
        std::ios::app
    );


    if(arquivo.tellp() == 0)
    {

        arquivo
        << "data,hora,evento,valor,duracao\n";

    }
    arquivo.close();
    // ==================================
    // Calibração
    // ==================================

    bool is_calibrating = true;
    int calibration_frames = 0;
    const int CALIBRATION_MAX = 60;

    double sum_calibration_ear = 0.0;
    double sum_calibration_mar = 0.0;

    double EAR_THRESHOLD = 0.0;
    double MAR_THRESHOLD = 0.0;

    const double DROP_FACTOR = 0.75;
    const double RISE_FACTOR = 1.35;

    const int CONSECUTIVE_FRAMES = 20;
    int frame_counter = 0;
    const int WINDOW_SIZE = 5;
    std::deque<double> ear_history;
    int yawn_counter = 0;
    const int YAWN_CONSECUTIVE_FRAMES = 15;
    bool sleep_saved = false;
    bool yawn_saved = false;

    // continua...


    // ==================================
    // Carregamento dos modelos
    // ==================================


    cv::CascadeClassifier face_detector;


    if(!face_detector.load("haarcascade_frontalface_alt2.xml"))
    {

        std::cerr 
        << "Erro ao carregar Haar Cascade"
        << std::endl;


        lgGpiochipClose(h_gpio);

        return -1;
    }




    cv::Ptr<cv::face::Facemark> facemark =
        cv::face::createFacemarkLBF();



    try
    {

        facemark->loadModel("lbfmodel.yaml");

    }

    catch(const cv::Exception& e)
    {

        std::cerr
        << "Erro modelo LBF: "
        << e.what()
        << std::endl;


        lgGpiochipClose(h_gpio);

        return -1;

    }





    // ==================================
    // Camera
    // ==================================


    cv::VideoCapture cap(0);


    if(!cap.isOpened())
    {

        std::cerr
        << "Erro ao abrir camera"
        << std::endl;


        return -1;

    }



    cv::Mat frame, gray;



    std::cout
    << "Monitor iniciado..."
    << std::endl;





    // ==================================
    // LOOP PRINCIPAL
    // ==================================


    while(true)
    {


        cap >> frame;



        if(frame.empty())
            break;




        cv::cvtColor(
            frame,
            gray,
            cv::COLOR_BGR2GRAY
        );



        cv::equalizeHist(
            gray,
            gray
        );





        std::vector<cv::Rect> faces;



        face_detector.detectMultiScale(
            gray,
            faces,
            1.1,
            4,
            0,
            cv::Size(100,100)
        );





        if(faces.empty())
        {


            if(!is_calibrating)
            {

                frame_counter = 0;

                yawn_counter = 0;


                sleep_saved = false;

                yawn_saved = false;



                lgGpioWrite(
                    h_gpio,
                    PIN_BUZZER,
                    0
                );


                lgGpioWrite(
                    h_gpio,
                    PIN_LED_SONOLENCIA,
                    0
                );

            }

        }

        else
        {


            std::vector<std::vector<cv::Point2f>> landmarks;


            bool success =
                facemark->fit(
                    frame,
                    faces,
                    landmarks
                );




            if(success)
            {


                std::vector<cv::Point2f>& shape =
                    landmarks[0];



                std::vector<cv::Point2f> left_eye;
                std::vector<cv::Point2f> right_eye;
                std::vector<cv::Point2f> mouth;




                for(int i=36;i<=41;i++)
                    left_eye.push_back(shape[i]);



                for(int i=42;i<=47;i++)
                    right_eye.push_back(shape[i]);



                for(int i=60;i<=67;i++)
                    mouth.push_back(shape[i]);






                double ear_raw =
                (
                    eye_aspect_ratio(left_eye)
                    +
                    eye_aspect_ratio(right_eye)

                ) / 2.0;



                double mar_raw =
                    mouth_aspect_ratio(mouth);





                ear_history.push_back(
                    ear_raw
                );



                if(ear_history.size() > WINDOW_SIZE)
                {

                    ear_history.pop_front();

                }




                double ear_smooth =
                    std::accumulate(
                        ear_history.begin(),
                        ear_history.end(),
                        0.0
                    )
                    /
                    ear_history.size();







                // ==================================
                // CALIBRAÇÃO
                // ==================================


                if(is_calibrating)
                {


                    sum_calibration_ear += ear_smooth;


                    sum_calibration_mar += mar_raw;



                    calibration_frames++;




                    cv::putText(
                        frame,
                        "CALIBRANDO...",
                        cv::Point(20,50),
                        cv::FONT_HERSHEY_SIMPLEX,
                        0.7,
                        cv::Scalar(0,255,255),
                        2
                    );






                    if(calibration_frames >= CALIBRATION_MAX)
                    {


                        double avgEAR =
                        sum_calibration_ear /
                        CALIBRATION_MAX;



                        double avgMAR =
                        sum_calibration_mar /
                        CALIBRATION_MAX;




                        EAR_THRESHOLD =
                            avgEAR *
                            DROP_FACTOR;




                        MAR_THRESHOLD =
                            avgMAR *
                            RISE_FACTOR;




                        is_calibrating=false;




                        std::cout
                        << "Calibracao finalizada\n"
                        << "EAR limite: "
                        << EAR_THRESHOLD
                        << "\nMAR limite: "
                        << MAR_THRESHOLD
                        << std::endl;




                        lgGpioWrite(
                            h_gpio,
                            PIN_BUZZER,
                            1
                        );


                        usleep(200000);



                        lgGpioWrite(
                            h_gpio,
                            PIN_BUZZER,
                            0
                        );

                    }


                }





                else
                {



                    bool eye_alert=false;

                    bool yawn_alert=false;





                    // ==================================
                    // OLHO FECHADO
                    // ==================================


                    if(ear_smooth < EAR_THRESHOLD)
                    {


                        frame_counter++;




                        if(frame_counter >= CONSECUTIVE_FRAMES)
                        {


                            eye_alert=true;



                            if(!sleep_saved)
                            {


                                salvar_evento(
                                    "SONOLENCIA",
                                    ear_smooth,
                                    frame_counter
                                );


                                sleep_saved=true;

                            }




                            static bool buzzer_state=false;



                            static auto last_toggle =
                            std::chrono::steady_clock::now();



                            auto now =
                            std::chrono::steady_clock::now();




                            if(
                            std::chrono::duration_cast<
                            std::chrono::milliseconds>
                            (now-last_toggle)
                            .count()
                            >=200)
                            {


                                buzzer_state =
                                    !buzzer_state;



                                lgGpioWrite(
                                    h_gpio,
                                    PIN_BUZZER,
                                    buzzer_state
                                    );



                                last_toggle=now;

                            }

                        }

                    }


                    else
                    {


                        frame_counter=0;


                        sleep_saved=false;



                        lgGpioWrite(
                            h_gpio,
                            PIN_BUZZER,
                            0
                        );

                    }







                    // ==================================
                    // BOCEJO
                    // ==================================


                    if(mar_raw > MAR_THRESHOLD)
                    {


                        yawn_counter++;



                        if(yawn_counter >= YAWN_CONSECUTIVE_FRAMES)
                        {


                            yawn_alert=true;




                            if(!yawn_saved)
                            {


                                salvar_evento(
                                    "BOCEJO",
                                    mar_raw,
                                    yawn_counter
                                );



                                yawn_saved=true;

                            }


                        }


                    }

                    else
                    {


                        yawn_counter=0;


                        yawn_saved=false;

                    }








                    // ==================================
                    // SAÍDA HARDWARE
                    // ==================================


                    if(
                        eye_alert ||
                        yawn_alert
                    )
                    {


                        lgGpioWrite(
                            h_gpio,
                            PIN_LED_SONOLENCIA,
                            1
                        );


                    }

                    else
                    {


                        lgGpioWrite(
                            h_gpio,
                            PIN_LED_SONOLENCIA,
                            0
                        );

                    }





                    cv::putText(
                        frame,
                        "EAR: "
                        +std::to_string(ear_smooth),
                        cv::Point(10,60),
                        cv::FONT_HERSHEY_SIMPLEX,
                        0.6,
                        cv::Scalar(255,255,0),
                        2
                    );



                    cv::putText(
                        frame,
                        "MAR: "
                        +std::to_string(mar_raw),
                        cv::Point(10,90),
                        cv::FONT_HERSHEY_SIMPLEX,
                        0.6,
                        cv::Scalar(255,255,0),
                        2
                    );


                }






                // Desenhar olhos

                for(int i=0;i<6;i++)
                {

                    cv::line(
                        frame,
                        left_eye[i],
                        left_eye[(i+1)%6],
                        cv::Scalar(0,255,0),
                        1
                    );


                    cv::line(
                        frame,
                        right_eye[i],
                        right_eye[(i+1)%6],
                        cv::Scalar(0,255,0),
                        1
                    );

                }





                // Desenhar boca

                for(int i=0;i<8;i++)
                {


                    cv::line(
                        frame,
                        mouth[i],
                        mouth[(i+1)%8],
                        cv::Scalar(255,0,255),
                        1
                    );

                }

            }

        }





        cv::imshow(
            "Monitor de Atencao",
            frame
        );




        if(cv::waitKey(1)=='q')
            break;


    }






    // ==================================
    // Encerramento
    // ==================================


    lgGpioWrite(
        h_gpio,
        PIN_LED_SONOLENCIA,
        0
    );


    lgGpioWrite(
        h_gpio,
        PIN_BUZZER,
        0
    );



    lgGpiochipClose(h_gpio);



    cap.release();


    cv::destroyAllWindows();



    return 0;

}