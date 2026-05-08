% Simulação de Cálculo de EAR e Detecção de Sonolência
clear; clc;

% 1. Parâmetros de Simulação
fs = 30;           % Amostragem (30 FPS)
t = 0:1/fs:10;     % 10 segundos de simulação
n_points = length(t);

% 2. Gerar sinal sintético de EAR
% EAR normal costuma variar entre 0.25 e 0.35.
% EAR de olho fechado costuma ser < 0.20.
ear_signal = 0.3 + 0.02 * randn(1, n_points); % Ruído normal

% Simular piscadas rápidas (frames 50-55)
ear_signal(50:55) = 0.15;

% Simular um episódio de sono (olho fechado por 2 segundos a partir do frame 150)
ear_signal(150:210) = 0.12;

% Filtro de média móvel para suavizar o sinal (Moving Average)
window_size = 3;
ear_smoothed = filter(ones(1, window_size)/window_size, 1, ear_signal);

% 3. Algoritmo de Detecção
threshold = 0.2;
consecutive_frames_limit = 15; % Limiar de tempo para atenção difusa
counter = 0;
alerts = zeros(1, n_points);

for i = 1:n_points
    if ear_smoothed(i) < threshold
        counter = counter + 1;
        if counter >= consecutive_frames_limit
            alerts(i) = 1; % Alerta ativado
        end
    else
        counter = 0;
    end
end

% 4. Visualização Gráfica
figure;
subplot(2,1,1);
plot(t, ear_smoothed, 'b', 'LineWidth', 1.5);
hold on;
line([0 10], [threshold threshold], 'Color', 'r', 'LineStyle', '--');
title('Simulação de Sinal EAR (Eye Aspect Ratio)');
ylabel('Valor EAR');
grid on;

subplot(2,1,2);
area(t, alerts, 'FaceColor', [1 0.8 0.8], 'EdgeColor', 'r');
title('Estado do Detector de Atenção');
xlabel('Tempo (s)');
ylabel('Alerta (0 ou 1)');
ylim([0 1.2]);
grid on;
