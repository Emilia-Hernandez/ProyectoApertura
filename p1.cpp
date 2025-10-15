#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <cctype> 
#include <filesystem>
#include <iomanip>


using namespace std;
using std::string;
using std::vector;

// -------- header helper (sin filesystem) --------
static bool need_header(const std::string& path) {
    std::ifstream fin(path, std::ios::binary);
    if (!fin.good()) return true;              // no existe o no abre
    fin.seekg(0, std::ios::end);
    return fin.tellg() == 0;                   // vacío
}

// -------- método pedido --------
void append_benchmark_csv(const std::string& csv_path,
                          int DB,
                          int threads,
                          const std::string& algoritmo,
                          double tiempo_s,
                          double speedup)
{
    static std::mutex m;                       // para evitar colisiones si se llama concurrente
    std::lock_guard<std::mutex> lk(m);

    const bool write_header = need_header(csv_path);

    std::ofstream out(csv_path, std::ios::app);
    if (!out) {
        std::cerr << "[ERROR] No pude abrir " << csv_path << " para escribir.\n";
        return;
    }

    if (write_header) {
        out << "DB,threads,algoritmo,tiempo_s,speedup\n";
    }

    // Por si alguien pasa threads<=0: para serial = 1
    int th = threads;
    if (th <= 0 && algoritmo == "serial") th = 1;

    out << DB << ','
        << th << ','
        << algoritmo << ','
        << std::fixed << std::setprecision(6) << tiempo_s << ','
        << std::fixed << std::setprecision(6) << speedup
        << '\n';

    out.flush();
}
vector<vector<double>> csvToMatrix(string path) {
    vector<vector<double>> M;

    std::ifstream file(path);
    if (!file) {
        std::cerr << "No pude abrir el archivo: " << path << "\n";
        return M;
    }

    string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        string cell;
        vector<double> row;

        while (std::getline(ss, cell, ',')) {
            size_t i = 0, j = cell.size();
            while (i < j && isspace(static_cast<unsigned char>(cell[i]))) ++i;
            while (j > i && isspace(static_cast<unsigned char>(cell[j-1]))) --j;
            string trimmed = cell.substr(i, j - i);

            if (!trimmed.empty()) {            // evita stod("") 
                row.push_back(std::stod(trimmed));
            }
        }
        if (!row.empty()) {
            M.emplace_back(std::move(row));
        }
    }
    return M;
}
vector<vector<double>> serial(double epsilon, int minPts, const vector<vector<double>>& M)
{
    int pts;
    double dist;
    bool reach; 
    int j; 
    vector<int> label(M.size(), -1); // Inicializa todas las etiquetas como -1 (no-core)
    vector<vector<double>> outliers; //creo un vector de etiquetas para cada punto

    //Identificar los primeros puntos core
    // 1 = core, -1 = no-core

    //no puedo poner los pushes en el for que paralelizo 
    //porque se hacen condiciones de carrera
    
    for (int i = 0; i < (int)M.size(); i++)
    {
        pts = 0;
        j = 0; 
        while(j<M.size() && pts<minPts)
        {
            if (j != i)
            {
                dist = sqrt(pow(M[i][0] - M[j][0], 2) + pow(M[i][1] - M[j][1], 2));
                if (dist <= epsilon)
                    pts++;
            }
            j++; 
        }
        if (pts >= minPts)   
            label[i]= 1;
    }

    //Identificar los epsilon alcanzables de los puntos core
    
    for (int i = 0; i < (int)M.size(); i++)
    { 
        if (label[i] == -1) // revisamos solo los no-core
        {
            reach = false;
            // buscar al menos un vecino que ya sea core
            j = 0; 
            while(reach == false && j<M.size())
            {
                if (label[j] == 1) // comparar solo contra cores
                {
                    dist = sqrt(pow(M[i][0] - M[j][0], 2) + pow(M[i][1] - M[j][1], 2));
                    if (dist <= epsilon)
                        reach = true;
                }
                j++;
            }
            if (reach)
                label[i] = 1; // alcanzable por core => lo tratamos como core/borde

        }
        
    }

    
    for(int i = 0; i < M.size(); i++)
    {
        if(label[i] == -1)
            outliers.push_back(M[i]);
    }

    return outliers;
}
vector<vector<double>> p1(double epsilon, int minPts, const vector<vector<double>>& M)
{
    //primera versión paralela en la que se usa todo el espacio como indivisible
    int pts;
    double dist;
    bool reach; 
    int j; 
    vector<int> label(M.size(), -1); // Inicializa todas las etiquetas como -1 (no-core)
    vector<vector<double>> outliers; //creo un vector de etiquetas para cada punto

    //Identificar los primeros puntos core
    // 1 = core, -1 = no-core

    //no puedo poner los pushes en el for que paralelizo 
    //porque se hacen condiciones de carrera
    #pragma omp parallel for shared(M,label,epsilon,minPts) private(j, pts,dist)
    for (int i = 0; i < (int)M.size(); i++)
    {
        pts = 0;
        j = 0; 
        while(j<M.size() && pts<minPts)
        {
            if (j != i)
            {
                dist = sqrt(pow(M[i][0] - M[j][0], 2) + pow(M[i][1] - M[j][1], 2));
                if (dist <= epsilon)
                    pts++;
            }
            j++; 
        }
        if (pts >= minPts)   
            label[i]= 1;
    }

    //Identificar los epsilon alcanzables de los puntos core
    #pragma omp parallel for shared(M,label,epsilon) private(j, dist,reach)
    for (int i = 0; i < (int)M.size(); i++)
    { 
        if (label[i] == -1) // revisamos solo los no-core
        {
            reach = false;
            // buscar al menos un vecino que ya sea core
            j = 0; 
            while(reach == false && j<M.size())
            {
                if (label[j] == 1) // comparar solo contra cores
                {
                    dist = sqrt(pow(M[i][0] - M[j][0], 2) + pow(M[i][1] - M[j][1], 2));
                    if (dist <= epsilon)
                        reach = true;
                }
                j++;
            }
            if (reach)
                label[i] = 1; // alcanzable por core => lo tratamos como core/borde

        }
        
    }

    //este tiene que ser serial pq si no hay condiciones de carrera
    for(int i = 0; i < M.size(); i++)
    {
        if(label[i] == -1)
            outliers.push_back(M[i]);
    }

    return outliers;
}
vector<vector<double>> p2(double epsilon, int minPts, int n,  const vector<vector<double>>& M)
{
    //segunda versión paralela en la que didide el espacio en n partes
    int pts;
    double dist;
    bool reach; 
    int j; 
    vector<int> label(M.size(), -1); // Inicializa todas las etiquetas como -1 (no-core)
    vector<vector<double>> outliers; //creo un vector de etiquetas para cada punto

    //Identificar los primeros puntos core
    // 1 = core, -1 = no-core

    // Elegir xcuts * ycuts = n, con factores lo más parejos posible
    int xcuts = (int)std::floor(std::sqrt(n));
    while (n % xcuts != 0) --xcuts;   // siempre termina (al menos en 1)
    int ycuts = n / xcuts;

    double splitx = 1.0 / xcuts;
    double splity = 1.0 / ycuts;

    // Ahora sí, hay exactamente N bloques
    std::vector<std::vector<int>> blocks(xcuts * ycuts);

    for (int i = 0; i < (int)M.size(); ++i) {
        // clamping para el borde derecho/superior
        int bx = std::min((int)(M[i][0] / splitx), xcuts - 1);
        int by = std::min((int)(M[i][1] / splity), ycuts - 1);
        int block_id = bx + by * xcuts;   // numeración fila mayor
        blocks[block_id].push_back(i);
    }   
    
    #pragma omp parallel for shared(M,label,epsilon,minPts) private(j, pts,dist)
    for (int i = 0; i < n; i++) //recorremos bloques y eso es lo que hacemos para
    {
        for (int k = 0; k < blocks[i].size(); k++)
        {
            pts = 0;
            j= 0; 
            while(j<blocks[i].size() && pts<minPts)
            {
                if(k!=j){
                    dist = sqrt(pow(M[blocks[i][k]][0] - M[blocks[i][j]][0], 2) + pow(M[blocks[i][k]][1] - M[blocks[i][j]][1], 2));
                    if (dist <= epsilon)
                        pts++;
                }
                j++; 
            }
            if (pts >= minPts)
                label[blocks[i][k]] = 1;
        }
        
    }

    //Identificar los epsilon alcanzables de los puntos core
    #pragma omp parallel for shared(M,label,epsilon) private(j, dist,reach)
    for (int i = 0; i < (int)M.size(); i++)
    { 
        if (label[i] == -1) // revisamos solo los no-core
        {
            reach = false;
            // buscar al menos un vecino que ya sea core
            j = 0; 
            while(reach == false && j<M.size())
            {
                if (label[j] == 1) // comparar solo contra cores
                {
                    dist = sqrt(pow(M[i][0] - M[j][0], 2) + pow(M[i][1] - M[j][1], 2));
                    if (dist <= epsilon)
                        reach = true;
                }
                j++;
            }
            if (reach)
                label[i] = 1; // alcanzable por core => lo tratamos como core/borde

        }
        
    }

    //este tiene que ser serial pq si no hay condiciones de carrera
    for(int i = 0; i < M.size(); i++)
    {
        if(label[i] == -1)
            outliers.push_back(M[i]);
    }

    return outliers;
}
void matrixToCsv(const vector<vector<double>>& M, const vector<vector<double>>& outliers, const string& path) 
{
    // Crear un set para búsqueda rápida de outliers
    set<pair<double, double>> outlierSet;
    for (const auto& pt : outliers) {
        outlierSet.insert({pt[0], pt[1]});
    }

    ofstream file(path);
    if (!file) {
        cerr << "No pude abrir el archivo para escribir\n";
        return;
    }

    // Escribir encabezado
    file << "x,y,tag\n";

    for (const auto& pt : M) {
        string tag = (outlierSet.count({pt[0], pt[1]}) > 0) ? "outlier" : "core";
        file << pt[0] << "," << pt[1] << "," << tag << "\n";
    }

    file.close();
}

static int db_from_path(const std::string& p) {
    size_t pos = p.find_last_of('/');
    if (pos == std::string::npos) pos = 0; else pos++;
    size_t underscore = p.find('_', pos);
    return std::stoi(p.substr(pos, underscore - pos));
}

int main()
{
    const std::string CSV_PATH = "resultados_benchmarks.csv";

    double start, end; //para medir tiempos
    double timeposerial; 
    std::vector<std::vector<double>> M;
    std::vector<std::vector<double>> outliers;
    int numThread[4] ={1,5,11,22};

    std::string pathsIn[8]= {
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/20000_data.csv", 
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/40000_data.csv", 
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/80000_data.csv", 
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/120000_data.csv", 
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/140000_data.csv", 
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/160000_data.csv", 
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/180000_data.csv", 
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/200000_data.csv"
    };
    
    std::string pathsOut[8]= {
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/20000_CR.csv", 
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/40000_CR.csv", 
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/80000_CR.csv", 
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/120000_CR.csv", 
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/140000_CR.csv", 
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/160000_CR.csv", 
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/180000_CR.csv", 
        "/Users/emiliahernandez/Desktop/comp para/proyecto1/200000_CR.csv"
    };
    
    for (int i = 0; i < 8; i++) {
        M = csvToMatrix(pathsIn[i]);     
        std::cout << "Filas: " << M.size() << "\n";

        const int DB = db_from_path(pathsIn[i]);

        // repito 10 veces cada experimento para tener un promedio
        for (size_t t = 0; t < 10; t++) {

            // ---------- SERIAL ----------
            std::cout << "Puntos atípicos serial: ";
            start = omp_get_wtime();
            outliers = serial(.05, 5, M);
            end = omp_get_wtime();
            std::cout << outliers.size() << "\n";
            std::cout << "Tiempo serial: " << (end - start) << " segundos\n";
            timeposerial = end - start;

            // log CSV (threads=1 para serial)
            append_benchmark_csv(CSV_PATH, DB, 1, "serial", timeposerial, 1.0);

            // ---------- PARALELOS (p1 y p2) ----------
            for (int j = 0; j < 4; j++) {
                omp_set_num_threads(numThread[j]);
                std::cout << "Número de threads: " << numThread[j] << "\n";

                // ---- p1 ----
                std::cout << "Puntos atípicos p1: ";
                start = omp_get_wtime();
                outliers = p1(.05, 5, M);
                end = omp_get_wtime();
                double tp1 = (end - start);
                std::cout << outliers.size() << "\n";
                std::cout << "Tiempo p1: " << tp1 << " segundos\n";
                std::cout << "Speedup p1: " << (timeposerial / tp1) << "\n";

                // log CSV p1
                append_benchmark_csv(CSV_PATH, DB, numThread[j], "p1", tp1, timeposerial / tp1);

                // ---- p2 ----
                std::cout << "Puntos atípicos p2: ";
                start = omp_get_wtime();
                outliers = p2(.05, 5, 4, M); // épsilon .05, min puntos 5, 4 subespacios
                end = omp_get_wtime();
                double tp2 = (end - start);
                std::cout << outliers.size() << "\n";
                std::cout << "Tiempo p2: " << tp2 << " segundos\n";
                std::cout << "Speedup p2: " << (timeposerial / tp2) << "\n";

                // log CSV p2
                append_benchmark_csv(CSV_PATH, DB, numThread[j], "p2", tp2, timeposerial / tp2);
            }
        }

        matrixToCsv(M, outliers, pathsOut[i]);
    }
    return 0;
}