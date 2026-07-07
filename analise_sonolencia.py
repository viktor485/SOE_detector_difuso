import pandas as pd


# ======================================
# Carregar CSV
# ======================================

arquivo = "eventos_sonolencia.csv"


try:
    dados = pd.read_csv(arquivo)

except FileNotFoundError:
    print("Arquivo CSV não encontrado!")
    exit()



# ======================================
# Mostrar dados brutos
# ======================================

print("\n==============================")
print(" DADOS REGISTRADOS")
print("==============================\n")


print(dados)



# ======================================
# Estatísticas gerais
# ======================================


total_eventos = len(dados)


bocejos = len(
    dados[dados["event"] == "BOCEJO"]
)


sonolencia = len(
    dados[dados["event"] == "SONOLENCIA"]
)



print("\n==============================")
print(" RELATÓRIO DE SONOLÊNCIA")
print("==============================\n")



print(
    f"Total de eventos: {total_eventos}"
)


print(
    f"Bocejos detectados: {bocejos}"
)


print(
    f"Sonolências detectadas: {sonolencia}"
)



# ======================================
# Análise dos valores
# ======================================


print("\n------------------------------")
print(" Valores dos indicadores")
print("------------------------------")



media_valor = dados["valor"].mean()


print(
    f"Média geral EAR/MAR: {media_valor:.3f}"
)




if sonolencia > 0:

    media_ear = dados[
        dados["event"] == "SONOLENCIA"
    ]["valor"].mean()


    print(
        f"EAR médio em sonolência: {media_ear:.3f}"
    )




if bocejos > 0:

    media_mar = dados[
        dados["event"] == "BOCEJO"
    ]["valor"].mean()


    print(
        f"MAR médio em bocejos: {media_mar:.3f}"
    )





# ======================================
# Evento mais frequente
# ======================================


evento_comum = (
    dados["event"]
    .value_counts()
    .idxmax()
)



print("\n------------------------------")

print(
    "Evento mais frequente:",
    evento_comum
)





# ======================================
# Histórico temporal
# ======================================


print("\n------------------------------")
print(" Histórico")
print("------------------------------\n")



for index, linha in dados.iterrows():

    print(
        f"{linha['data']} "
        f"{linha['hora']} -> "
        f"{linha['event']} "
        f"(valor={linha['valor']})"
    )



print("\nAnálise finalizada.")


