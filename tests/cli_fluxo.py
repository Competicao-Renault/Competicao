"""Verifica argumentos do executavel e caminhos UTF-8 de entrada e saida."""
import json
import subprocess
import tempfile
from pathlib import Path
from verificar_simulacao import conferir

exe = str(Path('bin/competicao.exe').resolve())
invalidos = [[], ['--desconhecido'], ['--semente'],
             ['--semente', '-1'], ['--semente', '4294967296'], ['--semente', '1x'],
             ['--construcao-segundos', '-1'], ['--refinamento-segundos', 'nan'],
             ['--construcao-segundos', '1x']]
for args in invalidos:
    comando = [exe, *args]
    if args:
        comando.append('instancia/alliances.json')
    resultado = subprocess.run(comando, capture_output=True)
    assert resultado.returncode != 0, (args, resultado.stdout, resultado.stderr)
assert subprocess.run([exe, '--help'], capture_output=True).returncode == 0
with tempfile.TemporaryDirectory(dir='bin') as pasta:
    entrada = Path(pasta) / 'entrada-\u00e7.json'
    saida = Path(pasta) / 'saida-\u00e7.json'
    instancia = json.loads(Path('instancia/alliances.json').read_text(encoding='utf-8'))
    entrada.write_text(json.dumps(instancia, ensure_ascii=False), encoding='utf-8')
    resultado = subprocess.run([exe, '--sem-sa', '--saida', str(saida), '--semente', '42',
                               '--construcao-segundos', '0', '--refinamento-segundos', '0',
                               str(entrada)], capture_output=True)
    assert resultado.returncode == 0, resultado.stderr.decode('utf-8', errors='replace')
    assert conferir(instancia, json.loads(saida.read_text()))[0] == 515357081
    assert b'SA=false' in resultado.stdout
    assert b'Tabu=false' in resultado.stdout
    invalido = subprocess.run([exe, '--saida', str(saida), str(entrada), str(entrada)], capture_output=True)
    assert invalido.returncode != 0
    for pesos in ['0,0,0,0,0,0,0,0', '-1,1,1,1,1,1,1,1', '1,2', '1,1,1,1,1,1,1,1,']:
        invalido = subprocess.run([exe, '--sa-pesos', pesos, '--construcao-segundos', '0', str(entrada)], capture_output=True)
        assert invalido.returncode != 0
    for arg, valor in [('--tabu-lista', '0'), ('--tabu-segundos', 'nan'), ('--tabu-max-propostas', '-1')]:
        invalido = subprocess.run([exe, '--sem-sa', arg, valor, '--construcao-segundos', '0', str(entrada)], capture_output=True)
        assert invalido.returncode != 0
    integrado = subprocess.run([exe, '--saida', str(saida), '--semente', '42', '--construcao-segundos', '0',
                               '--sa-segundos', '0.1', '--sa-max-propostas', '100',
                               '--tabu-segundos', '0.05',
                               '--refinamento-segundos', '0.05', str(entrada)], capture_output=True)
    assert integrado.returncode == 0, integrado.stderr.decode('utf-8', errors='replace')
    assert b'SA=true' in integrado.stdout and b'grupo: propostas=' in integrado.stdout
    assert b'Tabu=true' in integrado.stdout
    assert conferir(instancia, json.loads(saida.read_text()))[0] <= 515357081
print('20 verificacoes de CLI passaram, incluindo SA, tabu, refinamento e caminhos com acentos.')
