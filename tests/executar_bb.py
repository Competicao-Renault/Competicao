"""Compila e repete a campanha; congela os fontes antes de medir."""
import hashlib
import json
import subprocess
import sys
import zipfile
from pathlib import Path

raiz = Path(__file__).resolve().parents[1]
campanha = 'bb'
if len(sys.argv) > 1:
    campanha = sys.argv[1]
if len(sys.argv) > 2 or campanha not in ('bb', 'posicao', 'posicao_longa'):
    raise SystemExit('Uso: executar_bb.py [bb|posicao|posicao_longa]')
diretorio, driver, analise = 'bb', 'comparar_bb', 'analisar_bb'
argumentos_execucao, argumentos_analise = [], []
if campanha in ('posicao', 'posicao_longa'):
    diretorio, driver, analise = 'posicao_bb', 'comparar_posicao_bb', 'analisar_posicao_bb'
if campanha == 'posicao_longa':
    diretorio += '_longo'
    argumentos_execucao, argumentos_analise = ['prolongado'], ['output/' + diretorio]
pasta = raiz / 'output' / diretorio
pasta.mkdir(parents=True, exist_ok=True)
arquivos = sorted(p.relative_to(raiz) for d in ('src', 'include') for p in (raiz / d).rglob('*')
                  if p.suffix in ('.cpp', '.hpp', '.h') and p.name != 'main.cpp')
arquivos.append(Path('tests') / (driver + '.cpp'))
hashes = {str(p): hashlib.sha256((raiz / p).read_bytes()).hexdigest() for p in arquivos}
with zipfile.ZipFile(pasta / 'fontes.zip', 'w', zipfile.ZIP_DEFLATED) as arquivo:
    for p in arquivos:
        arquivo.write(raiz / p, p.as_posix())
(pasta / 'fontes.sha256').write_text(''.join(hashes[str(p)] + '  ' + p.as_posix() + '\n' for p in arquivos), encoding='utf-8')
executavel = 'bin/' + driver + '.exe'
fontes = [str(p) for p in arquivos if p.suffix == '.cpp']
comando = ['g++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Iinclude', '-Isrc', *fontes, '-o', executavel]
(pasta / 'configuracao.json').write_text(json.dumps({'comando': comando, 'execucao': [executavel, *argumentos_execucao], 'python': sys.version,
    'compilador': subprocess.run(['g++', '--version'], capture_output=True).stdout.decode(errors='replace').splitlines()[0]}, indent=2), encoding='utf-8')
(raiz / 'bin').mkdir(exist_ok=True)
subprocess.run(comando, cwd=raiz, check=True)
subprocess.run([executavel, *argumentos_execucao], cwd=raiz, check=True)
assert all(hashlib.sha256((raiz / p).read_bytes()).hexdigest() == hashes[str(p)] for p in arquivos), 'Fontes mudaram durante a campanha'
subprocess.run([sys.executable, 'tests/' + analise + '.py', *argumentos_analise], cwd=raiz, check=True)
