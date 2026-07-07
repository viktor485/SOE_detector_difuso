from flask import Flask, render_template
import pandas as pd
import os


app = Flask(__name__)


CSV_FILE = "eventos_sonolencia.csv"



def ler_dados():

    if not os.path.exists(CSV_FILE):
        return None


    dados = pd.read_csv(CSV_FILE)


    return dados





@app.route("/")
def index():


    dados = ler_dados()


    if dados is None or len(dados) == 0:

        return """
        <h1>Monitor de Sonolência</h1>
        <h2>Nenhum dado registrado</h2>
        """



    total = len(dados)


    bocejos = len(
        dados[dados["event"]=="BOCEJO"]
    )


    sonolencia = len(
        dados[dados["event"]=="SONOLENCIA"]
    )



    ultimo = dados.iloc[-1]



    pagina = f"""

    <html>

    <head>

    <title>
    Monitor de Sonolência
    </title>


    <style>

    body {{

        font-family: Arial;
        text-align:center;
        background:#f2f2f2;

    }}


    .card {{

        background:white;
        padding:20px;
        margin:20px;
        border-radius:10px;

    }}


    </style>


    </head>


    <body>


    <h1>
    Monitor de Atenção do Condutor
    </h1>



    <div class="card">

    <h2>
    Estatísticas
    </h2>


    <p>
    Total de eventos: {total}
    </p>


    <p>
    Bocejos: {bocejos}
    </p>


    <p>
    Sonolência crítica: {sonolencia}
    </p>


    </div>





    <div class="card">


    <h2>
    Último evento
    </h2>


    <p>
    Tipo: {ultimo['event']}
    </p>


    <p>
    Valor: {ultimo['valor']:.3f}
    </p>


    <p>
    Horário:
    {ultimo['data']}
    {ultimo['hora']}
    </p>


    </div>




    </body>

    </html>

    """



    return pagina







if __name__ == "__main__":


    app.run(
        host="0.0.0.0",
        port=5000
    )
