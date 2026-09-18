import os
from flask import Flask, render_template, request, send_from_directory, jsonify
from werkzeug.utils import secure_filename

app = Flask(__name__)
UPLOAD_FOLDER = 'uploads'
os.makedirs(UPLOAD_FOLDER, exist_ok=True)
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/process', methods=['POST'])
def process_files():
    if 'files' not in request.files:
        return jsonify({'error': 'No files uploaded'}), 400
    
    files = request.files.getlist('files')
    results = []

    for file in files:
        if file.filename == '':
            continue
        
        filename = secure_filename(file.filename)
        input_path = os.path.join(app.config['UPLOAD_FOLDER'], filename)
        file.save(input_path)

        base_name, ext = os.path.splitext(filename)
        envio_filename = f"{base_name}_envio{ext}"
        sympio_filename = f"{base_name}_sympio{ext}"
        
        envio_path = os.path.join(app.config['UPLOAD_FOLDER'], envio_filename)
        sympio_path = os.path.join(app.config['UPLOAD_FOLDER'], sympio_filename)

        if ext.lower() in ['.mp4', '.mov', '.avi', '.mkv']:
            # Envío Optimizado: Calidad alta, reducción de peso moderada (CRF 24)
            cmd_envio = f"ffmpeg -y -i '{input_path}' -vcodec libx264 -crf 24 -preset medium -acodec aac -b:a 128k '{envio_path}'"
            
            # Petruska Ultra: Compresión extrema inteligente (Escala a max 480p manteniendo proporción, preset slow para mejor calidad visual, CRF 28)
            cmd_sympio = f"ffmpeg -y -i '{input_path}' -vf \"scale='min(854,iw)':'min(480,ih)'\" -vcodec libx264 -crf 28 -preset slow -acodec aac -b:a 64k '{sympio_path}'"
            
            os.system(cmd_envio)
            os.system(cmd_sympio)
        else:
            os.system(f"cp '{input_path}' '{envio_path}'")
            os.system(f"cp '{input_path}' '{sympio_path}'")

        original_size = os.path.getsize(input_path) / (1024 * 1024)
        envio_size = os.path.getsize(envio_path) / (1024 * 1024)
        sympio_size = os.path.getsize(sympio_path) / (1024 * 1024)

        results.append({
            'original_name': filename,
            'envio_file': envio_filename,
            'sympio_file': sympio_filename,
            'original_mb': round(original_size, 2),
            'envio_mb': round(envio_size, 2),
            'sympio_mb': round(sympio_size, 2)
        })

    return jsonify({'results': results})

@app.route('/download/<filename>')
def download_file(filename):
    return send_from_directory(app.config['UPLOAD_FOLDER'], filename, as_attachment=True)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8000, debug=True)
