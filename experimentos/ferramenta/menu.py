"""Menu e consolidação da bancada experimental."""
import csv
import hashlib
import itertools
import json
import math
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path
from statistics import mean, stdev

sys.path.insert(0, str(Path("tests").resolve()))
from verificar_simulacao import conferir


INSTANCIAS = ("alliances", "reliability", "strikes")
SEMENTES = (7, 42, 2026, 11, 19)
OPERADORES = ("Aleatório", "Divisão", "Substituição", "Companhia", "Site", "Reutilização", "Grupo", "Quantidade")
PESOS = (1,) * 8
CONSTRUTIVO = (
    ("Máximo de blocos", 200, 0), ("Máximo de fluxos por bloco", 6, 1),
    ("Alternativas por fluxo", 8, 2), ("Propostas por bloco", 64, 1),
    ("Tamanho da lista restrita", 5, 1), ("Percentual aleatório do construtivo", 30, 0, 100),
    ("Tempo do construtivo (s)", 1.0, 0),
)
SA = (
    ("Temperatura inicial", 1000000.0, 0), ("Temperatura final", 10000.0, 0),
    ("Alfa", 0.99, 0, 1), ("Candidatos avaliados por nível (saMax)", 2000, 1),
    ("Máximo de propostas do SA", 100000000, 1), ("Tempo do SA (s)", 1.0, 0),
    ("Percentual aleatório do operador Grupo", 30, 0, 100),
)
TABU = (
    ("Tamanho da lista Tabu", 30, 1), ("Tamanho da vizinhança", 20, 1),
    ("Máximo de iterações", 1000, 0), ("Máximo de propostas da Tabu", 100000, 0),
    ("Tempo da Tabu (s)", 1.0, 0),
)
REFINAMENTO = (
    ("Máximo de grupos de refinamento", 100, 0), ("Tempo de refinamento (s; 0 desativa)", 0.5, 0),
    ("Alternativas por fluxo do refinamento", 6, 2), ("Máximo de combinações por grupo", 216, 1),
    ("Limite por grupo (s)", 0.2, 0),
)
MODOS = {
    1: ("Construtivo", (False, False, False)),
    2: ("Construtivo + SA", (True, False, False)),
    3: ("Construtivo + Tabu", (False, True, False)),
    4: ("Construtivo + refinamento", (False, False, True)),
    5: ("Construtivo + SA + refinamento", (True, False, True)),
    6: ("Construtivo + Tabu + refinamento", (False, True, True)),
    7: ("Comparar SA e Busca Tabu", (True, True, False)),
    8: ("Construtivo + SA + Tabu + refinamento", (True, True, True)),
}


def ler_csv(caminho):
    with caminho.open(encoding="utf-8", newline="") as arquivo:
        return list(csv.DictReader(arquivo))


def gravar_csv(caminho, linhas):
    with caminho.open("w", encoding="utf-8", newline="") as arquivo:
        escritor = csv.DictWriter(arquivo, fieldnames=linhas[0].keys())
        escritor.writeheader()
        escritor.writerows(linhas)


def ler_json(caminho):
    return json.loads(caminho.read_text(encoding="utf-8"))


def volume_spot(instancia, solucao):
    rotas = instancia["routes"]
    companhias = instancia["carriers"]
    return sum(a["n"] for a in solucao["x"] if companhias[rotas[a["route"]]["carrier"]]["name"] == "SPOT")


def numero(rotulo, padrao, minimo, maximo=None):
    if maximo is None and isinstance(padrao, int):
        maximo = 2**31 - 1
    while True:
        texto = input(f"{rotulo} [{padrao}]: ").strip()
        if not texto:
            return padrao
        try:
            valor = type(padrao)(texto)
            if math.isfinite(valor) and valor >= minimo and (maximo is None or valor <= maximo):
                return valor
        except (ValueError, OverflowError):
            pass
        print("Valor inválido para esse parâmetro.")


def valores(campos):
    return [numero(*campo) for campo in campos]


def padroes(campos):
    return [campo[1] for campo in campos]


def lista_inteiros(rotulo, padrao, minimo, maximo, validar):
    while True:
        texto = input(f"{rotulo} [{','.join(map(str, padrao))}]: ").strip()
        try:
            itens = list(map(int, texto.replace(",", " ").split())) if texto else list(padrao)
            if all(minimo <= n <= maximo for n in itens) and validar(itens):
                return itens
        except (ValueError, OverflowError):
            pass
        print("Lista inválida. Separe os números por vírgula ou espaço.")


def pesos_sa():
    print("Pesos do SA: 1 aleatório apenas; 2 iguais; 3 padrão; 4 personalizados.")
    opcao = numero("Configuração rápida", 3, 1, 4)
    if opcao == 4:
        print("Ordem: " + ", ".join(OPERADORES))
        return lista_inteiros("Oito pesos; zero desativa o operador", PESOS, 0, 2**31 - 1,
                              lambda itens: len(itens) == len(OPERADORES) and 0 < sum(itens) <= 2**31 - 1)
    return list({1: (1, 0, 0, 0, 0, 0, 0, 0), 2: (1,) * 8, 3: PESOS}[opcao])


def ler_configuracao(modo, indice):
    usar_sa, usar_tabu, usar_ref = MODOS[modo][1]
    print(f"\nConfiguração C{indice}")
    sa, tabu, ref, pesos = padroes(SA), padroes(TABU), padroes(REFINAMENTO), list(PESOS)
    niveis = 0
    if usar_sa:
        while True:
            sa = valores(SA if modo != 7 else SA[:5] + SA[6:])
            if modo == 7:
                sa.insert(5, 1.0)
            if sa[0] > sa[1] > 0 and 0 < sa[2] < 1 and sa[5] > 0:
                break
            print("Use temperatura inicial > final > 0, alfa entre 0 e 1 e tempo positivo.")
        pesos = pesos_sa()
        niveis = math.ceil((math.log(sa[0]) - math.log(sa[1])) / -math.log(sa[2]))
        niveis = max(1, niveis)
        print(f"Resfriamento previsto: {niveis} níveis, {niveis * sa[3]} candidatos avaliados.")
    if usar_tabu:
        tabu = valores(TABU if modo != 7 else TABU[:4])
    if modo == 7:
        while True:
            tempo = numero("Orçamento temporal comum ao SA e à Tabu (s)", 1.0, 0)
            if tempo > 0:
                break
            print("O orçamento comum deve ser positivo.")
        sa[5] = tempo
        tabu.append(tempo)
    if usar_ref:
        ref = valores(REFINAMENTO)
        while ref[1] > 0 and ref[0] > 0 and ref[4] <= 0:
            ref[4] = numero("Limite positivo por grupo (s)", 0.2, 0)
        usar_ref = ref[1] > 0
    base = {
        "nome": f"C{indice}", "sa": usar_sa, "tabu": usar_tabu, "refinamento": usar_ref,
        "parametros_sa": sa, "pesos": pesos, "parametros_tabu": tabu, "parametros_refinamento": ref,
        "niveis_previstos": niveis,
    }
    if modo == 7:
        return [dict(base, nome=f"C{indice}-{algoritmo}", sa=algoritmo == "sa", tabu=algoritmo == "tabu")
                for algoritmo in ("sa", "tabu")]
    return [base]


def ler_campanha(modo):
    print("Instâncias: 1 Alliances, 2 Reliability, 3 Strikes, 4 todas.")
    escolha = numero("Instância", 4, 1, 4)
    instancias = list(INSTANCIAS) if escolha == 4 else [INSTANCIAS[escolha - 1]]
    sementes = lista_inteiros("Sementes", SEMENTES, 0, 2**32 - 1,
                             lambda itens: bool(itens) and len(set(itens)) == len(itens))
    print("Parâmetros do construtivo compartilhado")
    construtivo = valores(CONSTRUTIVO)
    quantidade = numero("Quantas configurações nesta campanha", 1, 1)
    configs = [c for i in range(1, quantidade + 1) for c in ler_configuracao(modo, i)]
    return {"modo": modo, "instancias": instancias, "sementes": sementes, "construtivo": construtivo, "configuracoes": configs}


def linha(valores_linha):
    return " ".join(map(str, valores_linha))


def gravar_plano(plano, pasta):
    partes = [linha(plano["construtivo"])]
    for chave in ("instancias", "sementes"):
        partes.append(linha([len(plano[chave]), *plano[chave]]))
    partes.append(str(len(plano["configuracoes"])))
    for c in plano["configuracoes"]:
        partes.append(linha([c["nome"], int(c["sa"]), int(c["tabu"]), int(c["refinamento"]),
                             *c["parametros_sa"], *c["pesos"], *c["parametros_tabu"], *c["parametros_refinamento"]]))
    (pasta / "plano.txt").write_text("\n".join(partes) + "\n", encoding="utf-8")
    campos = {"construtivo": CONSTRUTIVO, "parametros_sa": SA, "parametros_tabu": TABU, "parametros_refinamento": REFINAMENTO}
    parametros = dict(plano, ordem_operadores=OPERADORES, campos={chave: [c[0] for c in grupo] for chave, grupo in campos.items()})
    (pasta / "parametros.json").write_text(json.dumps(parametros, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def verificar(pasta, plano):
    instancias = {nome: ler_json(Path("instancia") / f"{nome}.json") for nome in plano["instancias"]}
    iniciais = {}
    configs = {c["nome"]: c for c in plano["configuracoes"]}
    esperadas = set(itertools.product(instancias, map(str, plano["sementes"])))
    solucoes = pasta / "solucoes"
    for r in ler_csv(pasta / "construcoes.csv"):
        chave = (r["instancia"], r["semente"])
        inst = instancias[chave[0]]
        sol = ler_json(solucoes / r["json"])
        custo, _ = conferir(inst, sol)
        if chave in iniciais or custo != float(r["custo"]) or volume_spot(inst, sol) != int(r["spot"]):
            raise ValueError(f"Construção divergente: {chave}")
        iniciais[chave] = r
    if set(iniciais) != esperadas:
        raise ValueError("Construções ausentes ou inesperadas")

    operadores = {}
    for r in ler_csv(pasta / "operadores_sa.csv"):
        chave = (r["instancia"], r["semente"], r["configuracao"])
        operadores.setdefault(chave, []).append(r)
    rastreados = set()
    dados = ler_csv(pasta / "execucoes.csv")
    for r in dados:
        chave = (r["instancia"], r["semente"], r["configuracao"])
        inst = instancias[chave[0]]
        inicial = iniciais[chave[:2]]
        config = configs[chave[2]]
        if chave in rastreados or any(int(r[k]) != config[k] for k in ("sa", "tabu", "refinamento")):
            raise ValueError(f"Execução duplicada ou divergente: {chave}")
        rastreados.add(chave)
        for etapa, campo in (("sa", "custo_apos_sa"), ("tabu", "custo_apos_tabu"), ("final", "custo_final")):
            if not r[f"json_{etapa}"]:
                continue
            sol = ler_json(solucoes / r[f"json_{etapa}"])
            custo, pior = conferir(inst, sol)
            if custo != float(r[campo]):
                raise ValueError(f"Custo Python divergente: {chave}, {etapa}")
            if etapa == "final" and (pior != int(r["pior_cenario"]) or volume_spot(inst, sol) != int(r["spot_final"]) or r["viavel"] != "1"):
                raise ValueError(f"Solução final divergente: {chave}")
        if (float(r["custo_inicial"]) != float(inicial["custo"]) or int(r["spot_inicial"]) != int(inicial["spot"])
                or not float(r["custo_final"]) <= float(r["custo_apos_tabu"]) <= float(r["custo_apos_sa"]) <= float(r["custo_inicial"])):
            raise ValueError(f"Encadeamento de custos inválido: {chave}")
        ops = operadores.pop(chave, [])
        if config["sa"]:
            totais = [sum(int(o[campo]) for o in ops) for campo in ("propostas", "avaliados", "viaveis", "aceitos")]
            aceitos = sum(int(r[k]) for k in ("melhorias_sa", "empates_sa", "pioras_sa"))
            if ([int(o["peso"]) for o in ops] != config["pesos"] or
                    totais != [int(r["propostas_sa"]), int(r["avaliados_sa"]), int(r["viaveis_sa"]), aceitos] or
                    any(not 0 <= max(int(o["aceitos"]), int(o["viaveis"])) <= int(o["avaliados"]) <= int(o["propostas"]) or
                        (int(o["peso"]) == 0 and int(o["propostas"]) != 0) for o in ops)):
                raise ValueError(f"Contadores por operador divergentes: {chave}")
        elif ops:
            raise ValueError(f"Operadores registrados para configuração sem SA: {chave}")
        r["algoritmo"] = "+".join(["construtivo", *(k for k in ("sa", "tabu", "refinamento") if config[k])])
    if operadores or rastreados != {(*par, cfg) for par in esperadas for cfg in configs}:
        raise ValueError("Execuções ausentes ou inesperadas")
    gravar_csv(pasta / "execucoes.csv", dados)
    return dados


def consolidar(pasta, plano, dados):
    resumo, pares, resumo_pares, melhores, scores = [], [], [], [], []
    destino = pasta / "melhores"
    destino.mkdir()
    for instancia in plano["instancias"]:
        grupos = {}
        for c in plano["configuracoes"]:
            nome = c["nome"]
            grupo = [r for r in dados if r["instancia"] == instancia and r["configuracao"] == nome]
            grupos[nome] = {r["semente"]: r for r in grupo}
            custos = [float(r["custo_final"]) for r in grupo]
            tempos = [sum(float(r[k]) for k in ("tempo_construcao_s", "tempo_sa_s", "tempo_tabu_s", "tempo_refinamento_s")) for r in grupo]
            resumo.append(dict(instancia=instancia, configuracao=nome, algoritmo=grupo[0]["algoritmo"],
                               melhor=min(custos), media=mean(custos), pior=max(custos),
                               desvio_amostral=stdev(custos) if len(custos) > 1 else "", tempo_medio_s=mean(tempos)))
            melhor = min(grupo, key=lambda r: (float(r["custo_final"]), int(r["semente"])))
            arquivo = f"{instancia}-{nome}.json"
            shutil.copyfile(pasta / "solucoes" / melhor["json_final"], destino / arquivo)
            melhores.append(dict(instancia=instancia, configuracao=nome, semente=melhor["semente"],
                                 custo=melhor["custo_final"], json=arquivo))
        for a, b in itertools.combinations(grupos, 2):
            ganhos = []
            for semente in grupos[a]:
                inicial, posterior = grupos[a][semente], grupos[b][semente]
                ganho = float(inicial["custo_final"]) - float(posterior["custo_final"])
                ganhos.append(ganho)
                pares.append(dict(instancia=instancia, semente=semente, anterior=a, posterior=b, ganho=ganho,
                                  custo_anterior=inicial["custo_final"], custo_posterior=posterior["custo_final"]))
            resumo_pares.append(dict(instancia=instancia, anterior=a, posterior=b, ganho_medio=mean(ganhos),
                                     melhoraram=sum(g > 0 for g in ganhos), empataram=ganhos.count(0), pioraram=sum(g < 0 for g in ganhos)))
    if set(plano["instancias"]) == set(INSTANCIAS):
        for c in plano["configuracoes"]:
            nome = c["nome"]
            scores.append(dict(configuracao=nome, soma_melhores_custos=sum(float(r["custo"]) for r in melhores if r["configuracao"] == nome)))
    for nome, linhas in (("resumo", resumo), ("pares", pares), ("resumo_pares", resumo_pares), ("melhores", melhores), ("scores", scores)):
        if linhas:
            gravar_csv(pasta / f"{nome}.csv", linhas)
    print(f"\n{'Instância':<13} {'Configuração':<13} {'Melhor':>13} {'Média':>13} {'Pior':>13} {'DP amostral':>13} {'Tempo (s)':>11}")
    for r in resumo:
        dp = f"{r['desvio_amostral']:.2f}" if r["desvio_amostral"] != "" else "—"
        print(f"{r['instancia']:<13} {r['configuracao']:<13} {r['melhor']:>13.2f} {r['media']:>13.2f} "
              f"{r['pior']:>13.2f} {dp:>13} {r['tempo_medio_s']:>11.4f}")
    for r in resumo_pares:
        print(f"{r['instancia']} {r['anterior']} → {r['posterior']}: ganho médio {r['ganho_medio']:.2f}; "
              f"{r['melhoraram']} melhoras, {r['empataram']} empates, {r['pioraram']} pioras.")
    for r in scores:
        print(r["configuracao"])
        for melhor in melhores:
            if melhor["configuracao"] == r["configuracao"]:
                print(f"{melhor['instancia'].capitalize()} = {float(melhor['custo']):.2f}")
        print(f"SCORE TOTAL = {r['soma_melhores_custos']:.2f}")


def executar_com_log(comando, log):
    with subprocess.Popen(comando, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8") as processo:
        for linha_saida in processo.stdout:
            print(linha_saida, end="")
            log.write(linha_saida)
        if processo.wait():
            raise subprocess.CalledProcessError(processo.returncode, comando)


def hashes(arquivos):
    return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(arquivos)}


def rodar(plano, resultados=Path("experimentos/resultados")):
    resultados.mkdir(parents=True, exist_ok=True)
    prefixo = datetime.now().strftime("campanha-%Y%m%d-%H%M%S-")
    pasta = Path(tempfile.mkdtemp(prefix=prefixo, dir=resultados))
    gravar_plano(plano, pasta)
    fontes = sorted(p for p in Path("src").rglob("*.cpp") if p != Path("src/main.cpp"))
    bancada = Path(__file__).resolve().with_name("bancada.cpp").relative_to(Path.cwd())
    monitorados = [*fontes, *Path("src").rglob("*.hpp"), *Path("include").rglob("*.hpp"), bancada,
                   Path(__file__).resolve().relative_to(Path.cwd()), Path("experimentos/testar.sh"),
                   Path("tests/verificar_simulacao.py"),
                   *(Path("instancia") / f"{n}.json" for n in plano["instancias"])]
    iniciais = hashes(monitorados)
    (pasta / "fontes.sha256").write_text("".join(f"{h}  {p}\n" for p, h in iniciais.items()), encoding="utf-8")
    versao = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True)
    branch = subprocess.check_output(["git", "branch", "--show-current"], text=True)
    estado = subprocess.check_output(["git", "-c", "core.autocrlf=true", "status", "--short", "--", "src", "include", "tests"], text=True)
    compilador = subprocess.check_output(["g++", "--version"], text=True).splitlines()[0]
    (pasta / "versao.txt").write_text(f"{versao}{branch}{compilador}\n{estado}", encoding="utf-8")
    print(f"Resultados: {pasta}")
    with tempfile.TemporaryDirectory() as temp, (pasta / "execucao.log").open("w", encoding="utf-8") as log:
        executavel = Path(temp) / "bancada"
        comando = ["g++", "-std=c++17", "-O3", "-Wall", "-Wextra", "-Wpedantic",
                   "-Iinclude", "-Isrc", *map(str, fontes), str(bancada), "-o", str(executavel)]
        executar_com_log(comando, log)
        executar_com_log([str(executavel), str(pasta / "plano.txt"), str(pasta)], log)
        if hashes(monitorados) != iniciais:
            raise RuntimeError("Arquivos usados na campanha mudaram durante a execução.")
        dados = verificar(pasta, plano)
        consolidar(pasta, plano, dados)
    (pasta / "validado.txt").write_text(f"{len(dados)} execuções validadas em C++ e independentemente em Python.\n", encoding="utf-8")
    print(f"Campanha validada em {pasta}")
    return pasta


def main():
    while True:
        print("\nBancada experimental")
        for opcao, (nome, _) in MODOS.items():
            print(f"{opcao}. {nome}")
        print("9. E4 nas três instâncias (Construtivo + SA)")
        print("0. Sair")
        modo = numero("Escolha", 0, 0, 9)
        if modo == 0:
            return
        try:
            if modo == 9:
                sa = padroes(SA)
                sa[5] = numero("Tempo do SA (s)", sa[5], 0.001)
                e4 = dict(nome="E4", sa=True, tabu=False, refinamento=False,
                          parametros_sa=sa, pesos=list(PESOS), parametros_tabu=padroes(TABU),
                          parametros_refinamento=padroes(REFINAMENTO))
                rodar(dict(modo=modo, instancias=list(INSTANCIAS), sementes=list(SEMENTES),
                           construtivo=padroes(CONSTRUTIVO), configuracoes=[e4]))
            else:
                rodar(ler_campanha(modo))
        except (OSError, RuntimeError, ValueError, AssertionError, subprocess.CalledProcessError) as erro:
            print(f"Campanha interrompida: {erro}", file=sys.stderr)


if __name__ == "__main__":
    try:
        main()
    except (EOFError, KeyboardInterrupt):
        print("\nBancada encerrada.")
