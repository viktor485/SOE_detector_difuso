# Compilador a ser utilizado
CXX = g++

# Flags de compilacao (Otimizacao maxima e avisos)
# O $(shell ...) executa o comando no terminal e injeta o resultado aqui
CXXFLAGS = -O3 -Wall $(shell pkg-config --cflags opencv4)

# Bibliotecas (Linker flags)
LDFLAGS = $(shell pkg-config --libs opencv4) -lopencv_face -lwiringPi -llgpio

# Nome do arquivo executavel final
TARGET = detector_atencao

# Arquivos fonte
SRCS = atencaomotorista.cpp

# Transforma os arquivos .cpp em arquivos objeto .o
OBJS = $(SRCS:.cpp=.o)

# Regra padrao executada ao digitar apenas 'make'
all: $(TARGET)

# Regra para gerar o executavel
$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# Regra para compilar os arquivos objeto (.o)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Regra para limpar os arquivos compilados (make clean)
clean:
	rm -f $(OBJS) $(TARGET)

# Declara que 'all' e 'clean' nao sao arquivos
.PHONY: all clean
