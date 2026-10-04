import csv
import matplotlib.pyplot as plt
import numpy as np

# Configurar estilo visual limpio y legible
plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')
plt.rcParams['font.sans-serif'] = 'DejaVu Sans'
plt.rcParams['font.size'] = 10

# 1. Cargar datos de tiempos.csv
rows = []
with open('tiempos.csv', 'r', encoding='utf-8') as f:
    reader = csv.DictReader(f)
    for r in reader:
        rows.append(r)

# Filtramos las corridas de filtros individuales estándar para comparaciones justas
single_filters = ['blur', 'sharpen', 'laplace']
dataset_map = {
    'lena': {'PGM': {}, 'PPM': {}},
    'fruit': {'PGM': {}, 'PPM': {}},
    'puj': {'PGM': {}, 'PPM': {}},
}

for r in rows:
    f_name = r['filtros'].strip().lower()
    if f_name in single_filters:
        inp = r['archivo_entrada'].lower()
        key = None
        if 'lena' in inp: key = 'lena'
        elif 'fruit' in inp: key = 'fruit'
        elif 'puj' in inp: key = 'puj'
        
        fmt = 'PPM' if 'ppm' in r['formato'].lower() else 'PGM'
        if key:
            t = float(r['tiempo_total_s']) * 1000.0 # en milisegundos
            dataset_map[key][fmt][f_name] = t

# ==========================================
# GRÁFICA 1: Tiempos por Imagen y Filtro (PGM vs PPM)
# ==========================================
fig, axes = plt.subplots(1, 2, figsize=(14, 6), sharey=False)

# Subplot A: PGM (Escala de grises - 1 canal)
images = ['lena', 'fruit', 'puj']
x = np.arange(len(images))
width = 0.25

colors = {'blur': '#2563eb', 'sharpen': '#10b981', 'laplace': '#f59e0b'}

ax1 = axes[0]
for i, f_name in enumerate(single_filters):
    vals = [dataset_map[img]['PGM'].get(f_name, 0) for img in images]
    rects = ax1.bar(x + (i - 1) * width, vals, width, label=f_name.capitalize(), color=colors[f_name], edgecolor='black', alpha=0.9)
    for rect in rects:
        h = rect.get_height()
        if h > 0:
            ax1.annotate(f'{h:.1f}ms',
                         xy=(rect.get_x() + rect.get_width() / 2, h),
                         xytext=(0, 3), textcoords="offset points",
                         ha='center', va='bottom', fontsize=8, fontweight='bold')

ax1.set_title('PGM (P2 - Escala de Grises, 1 Canal)', fontsize=12, fontweight='bold')
ax1.set_xlabel('Imagen', fontweight='bold')
ax1.set_ylabel('Tiempo de Ejecución (ms)', fontweight='bold')
ax1.set_xticks(x)
ax1.set_xticklabels(['Lena\n(512x512)', 'Fruit\n(900x450)', 'PUJ\n(1920x600)'])
ax1.legend(title='Filtro')
ax1.grid(True, linestyle='--', alpha=0.5)

# Subplot B: PPM (Color - 3 canales RGB)
ax2 = axes[1]
for i, f_name in enumerate(single_filters):
    vals = [dataset_map[img]['PPM'].get(f_name, 0) for img in images]
    rects = ax2.bar(x + (i - 1) * width, vals, width, label=f_name.capitalize(), color=colors[f_name], edgecolor='black', alpha=0.9)
    for rect in rects:
        h = rect.get_height()
        if h > 0:
            ax2.annotate(f'{h:.1f}ms',
                         xy=(rect.get_x() + rect.get_width() / 2, h),
                         xytext=(0, 3), textcoords="offset points",
                         ha='center', va='bottom', fontsize=8, fontweight='bold')

ax2.set_title('PPM (P3 - Color RGB, 3 Canales)', fontsize=12, fontweight='bold')
ax2.set_xlabel('Imagen', fontweight='bold')
ax2.set_ylabel('Tiempo de Ejecución (ms)', fontweight='bold')
ax2.set_xticks(x)
ax2.set_xticklabels(['Lena\n(128x128)', 'Fruit\n(900x450)', 'PUJ\n(1920x600)'])
ax2.legend(title='Filtro')
ax2.grid(True, linestyle='--', alpha=0.5)

plt.suptitle('Diseño 2 (Secuencial): Comparativa de Tiempos de Filtrado por Imagen y Filtro', fontsize=14, fontweight='bold')
plt.tight_layout()
plt.savefig('images/grafica_tiempos_filtros.png', dpi=300)
plt.close()
print("Guardada: images/grafica_tiempos_filtros.png")

# ==========================================
# GRÁFICA 2: Escalabilidad - Tamaño de Imagen vs Tiempo Total
# ==========================================
plt.figure(figsize=(10, 6))

total_values = []
wall_times = []
labels = []
colors_scatter = []

for r in rows:
    f_name = r['filtros'].strip().lower()
    # Tomamos ejecuciones de un solo filtro para medir tamaño vs tiempo
    if f_name in single_filters:
        w = int(r['ancho'])
        h = int(r['alto'])
        c = int(r['canales'])
        n_vals = w * h * c
        t_ms = float(r['tiempo_total_s']) * 1000.0
        
        total_values.append(n_vals / 1e6) # en millones de valores (Mpx-canales)
        wall_times.append(t_ms)
        name = r['archivo_entrada'].split('/')[-1]
        labels.append(f"{name} ({f_name})")
        colors_scatter.append('#2563eb' if c == 1 else '#dc2626')

plt.scatter(total_values, wall_times, c=colors_scatter, s=90, alpha=0.8, edgecolors='black', zorder=5)

# Ajuste lineal O(N)
m, b = np.polyfit(total_values, wall_times, 1)
x_line = np.linspace(min(total_values), max(total_values), 100)
plt.plot(x_line, m * x_line + b, color='#475569', linestyle='--', linewidth=2, label=f'Tendencia lineal O(N) (R² alto)')

plt.title('Impacto del Tamaño de la Imagen (Millones de Valores) en el Tiempo de Ejecución', fontsize=13, fontweight='bold')
plt.xlabel('Millones de Valores Procesados (Ancho × Alto × Canales)', fontsize=11, fontweight='bold')
plt.ylabel('Tiempo Total de Ejecución (ms)', fontsize=11, fontweight='bold')

# Leyenda personalizada
from matplotlib.lines import Line2D
custom_lines = [
    Line2D([0], [0], marker='o', color='w', markerfacecolor='#2563eb', markersize=10, label='PGM (1 canal)'),
    Line2D([0], [0], marker='o', color='w', markerfacecolor='#dc2626', markersize=10, label='PPM (3 canales)'),
    Line2D([0], [0], color='#475569', linestyle='--', linewidth=2, label='Ajuste lineal O(N)')
]
plt.legend(handles=custom_lines, loc='upper left', frameon=True)
plt.grid(True, linestyle='--', alpha=0.6)
plt.tight_layout()
plt.savefig('images/grafica_escalabilidad_tamano.png', dpi=300)
plt.close()
print("Guardada: images/grafica_escalabilidad_tamano.png")

# ==========================================
# GRÁFICA 3: CPU Time vs Wall Time
# ==========================================
plt.figure(figsize=(9, 6))

cpu_times = [float(r['tiempo_cpu_s']) * 1000.0 for r in rows if r['filtros'].strip().lower() in single_filters]
wall_times = [float(r['tiempo_total_s']) * 1000.0 for r in rows if r['filtros'].strip().lower() in single_filters]

plt.scatter(cpu_times, wall_times, color='#7c3aed', s=80, edgecolors='black', alpha=0.85, zorder=5, label='Mediciones individuales')
max_v = max(max(cpu_times), max(wall_times)) * 1.05
plt.plot([0, max_v], [0, max_v], color='#dc2626', linestyle='--', linewidth=2, label='Línea ideal 1:1 (100% Monohilo Secuencial)')

plt.title('Tiempo CPU vs Tiempo Total (Wall-Clock Time) - Diseño Secuencial', fontsize=13, fontweight='bold')
plt.xlabel('Tiempo de CPU (ms)', fontsize=11, fontweight='bold')
plt.ylabel('Tiempo Total de Reloj (ms)', fontsize=11, fontweight='bold')
plt.legend(frameon=True)
plt.grid(True, linestyle='--', alpha=0.6)
plt.tight_layout()
plt.savefig('images/grafica_cpu_vs_wall.png', dpi=300)
plt.close()
print("Guardada: images/grafica_cpu_vs_wall.png")
