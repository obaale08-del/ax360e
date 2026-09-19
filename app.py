import os
import hashlib
import secrets
import subprocess
from PIL import Image
from flask import Flask, render_template, request, jsonify, send_from_directory, session, redirect, url_for

app = Flask(__name__)
app.secret_key = secrets.token_hex(32)

UPLOAD_FOLDER = 'uploads'
OPTIMIZED_FOLDER = 'optimizadas'
ENVIO_FOLDER = 'envios'
VAULT_FOLDER = 'vault_cifrada'

os.makedirs(UPLOAD_FOLDER, exist_ok=True)
os.makedirs(OPTIMIZED_FOLDER, exist_ok=True)
os.makedirs(ENVIO_FOLDER, exist_ok=True)
os.makedirs(VAULT_FOLDER, exist_ok=True)

USUARIO_ADMIN = "admin"
PASSWORD_HASH_ADMIN = hashlib.sha256("mamaguevo".encode()).hexdigest()

@app.after_request
def add_security_headers(response):
    response.headers['X-Content-Type-Options'] = 'nosniff'
    response.headers['X-Frame-Options'] = 'SAMEORIGIN'
    response.headers['X-XSS-Protection'] = '1; mode=block'
    return response

@app.route('/login', methods=['GET', 'POST'])
def login():
    error = None
    if request.method == 'POST':
        username = request.form.get('username', '')
        password = request.form.get('password', '')
        pass_hash = hashlib.sha256(password.encode()).hexdigest()
        
        if username == USUARIO_ADMIN and pass_hash == PASSWORD_HASH_ADMIN:
            session['logged_in'] = True
            session.permanent = True
            return redirect(url_for('index'))
        else:
            error = "Credenciales incorrectas."
    return render_template('login.html', error=error)

@app.route('/logout')
def logout():
    session.pop('logged_in', None)
    return redirect(url_for('login'))

@app.route('/')
def index():
    if not session.get('logged_in'):
        return redirect(url_for('login'))
        
    opt_dir = 'static/backgrounds/optimizadas'
    if os.path.exists(opt_dir) and os.listdir(opt_dir):
        bg_images = os.listdir(opt_dir)
    else:
        bg_images = os.listdir('static/backgrounds') if os.path.exists('static/backgrounds') else []
        
    return render_template('index.html', bg_images=bg_images)

@app.route('/process', methods=['POST'])
def process_files():
    if not session.get('logged_in'):
        return jsonify({'error': 'Unauthorized'}), 401
    if 'files' not in request.files:
        return jsonify({'error': 'No files uploaded'}), 400
    
    files = request.files.getlist('files')
    results = []
    
    for file in files:
        if file.filename == '':
            continue
        filename = os.path.basename(file.filename)
        filepath = os.path.join(UPLOAD_FOLDER, filename)
        file.save(filepath)
        
        original_size = os.path.getsize(filepath)
        orig_mb = round(original_size / (1024 * 1024), 2)
        
        opt_filepath = os.path.join(OPTIMIZED_FOLDER, filename)
        envio_filepath = os.path.join(ENVIO_FOLDER, filename)
        
        ext = filename.lower().split('.')[-1]
        
        print(f"\n[PROCESANDO] Archivo: {filename} (Extensión: {ext}) - Tamaño: {orig_mb} MB")

        try:
            if ext in ['png', 'jpg', 'jpeg', 'webp']:
                with Image.open(filepath) as img:
                    img.thumbnail((800, 800))
                    img.save(opt_filepath, quality=55, optimize=True)
                    img.thumbnail((1280, 1280))
                    img.save(envio_filepath, quality=75, optimize=True)
                print(" -> Imagen comprimida con éxito con Pillow.")
                
            elif ext in ['mp4', 'mkv', 'mov', '3gp', 'avi', 'webm']:
                cmd_opt = ['ffmpeg', '-y', '-i', filepath, '-vf', 'scale=-2:480', '-crf', '32', '-preset', 'fast', opt_filepath]
                res_opt = subprocess.run(cmd_opt, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                print(f" -> FFmpeg Ultrareducido código de salida: {res_opt.returncode}")
                if res_opt.returncode != 0:
                    print(f"    Error FFmpeg opt: {res_opt.stderr.decode('utf-8', errors='ignore')}")

                cmd_env = ['ffmpeg', '-y', '-i', filepath, '-vf', 'scale=-2:720', '-crf', '26', '-preset', 'fast', envio_filepath]
                res_env = subprocess.run(cmd_env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                print(f" -> FFmpeg Envío código de salida: {res_env.returncode}")
                if res_env.returncode != 0:
                    print(f"    Error FFmpeg env: {res_env.stderr.decode('utf-8', errors='ignore')}")
            else:
                with open(filepath, 'rb') as f_in, open(opt_filepath, 'wb') as f_out:
                    f_out.write(f_in.read())
                with open(filepath, 'rb') as f_in, open(envio_filepath, 'wb') as f_out:
                    f_out.write(f_in.read())
                print(" -> Archivo genérico copiado sin cambios.")
                
        except Exception as e:
            print(f"❌ EXCEPCIÓN CRÍTICA procesando archivo: {e}")
            with open(filepath, 'rb') as f_in, open(opt_filepath, 'wb') as f_out:
                f_out.write(f_in.read())
            with open(filepath, 'rb') as f_in, open(envio_filepath, 'wb') as f_out:
                f_out.write(f_in.read())

        opt_mb = round(os.path.getsize(opt_filepath) / (1024 * 1024), 2)
        envio_mb = round(os.path.getsize(envio_filepath) / (1024 * 1024), 2)
        
        results.append({
            'filename': filename,
            'orig_mb': orig_mb,
            'opt_mb': opt_mb,
            'envio_mb': envio_mb
        })
        
    return jsonify({'results': results})

@app.route('/download/opt/<filename>')
def download_opt(filename):
    if not session.get('logged_in'):
        return redirect(url_for('login'))
    return send_from_directory(OPTIMIZED_FOLDER, os.path.basename(filename), as_attachment=True)

@app.route('/download/envio/<filename>')
def download_envio(filename):
    if not session.get('logged_in'):
        return redirect(url_for('login'))
    return send_from_directory(ENVIO_FOLDER, os.path.basename(filename), as_attachment=True)

@app.route('/preview/<tipo>/<filename>')
def preview_file(tipo, filename):
    if not session.get('logged_in'):
        return redirect(url_for('login'))
    folder = OPTIMIZED_FOLDER if tipo == 'opt' else ENVIO_FOLDER
    return send_from_directory(folder, os.path.basename(filename))

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=False)
