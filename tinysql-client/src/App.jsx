import React, { useState } from 'react';
import './App.css';

function App() {
  const [sql, setSql] = useState('');
  const [resultados, setResultados] = useState([]);
  const [cargando, setCargando] = useState(false);
  const [currentDatabase, setCurrentDatabase] = useState('');
  const [serverUrl, setServerUrl] = useState('http://localhost:8080/query');

  const enviarAlServidor = async (sentencia, database) => {
    const payload = { sql: sentencia };
    if (database) payload.database = database;

    const inicio = performance.now();
    try {
      const response = await fetch(serverUrl, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      const data = await response.json();
      data.elapsed_ms = data.elapsed_ms || (performance.now() - inicio);
      return data;
    } catch (error) {
      return {
        success: false,
        error: `Error de conexión: ${error.message}`,
        elapsed_ms: performance.now() - inicio
      };
    }
  };

  const ejecutarConsultas = async () => {
    if (!sql.trim()) return;

    const sentencias = sql
      .split(';')
      .map(s => s.trim())
      .filter(s => s.length > 0);

    if (sentencias.length === 0) return;

    setCargando(true);
    const nuevosResultados = [];
    let dbActual = currentDatabase;

    for (const sentencia of sentencias) {
      const respuesta = await enviarAlServidor(sentencia, dbActual);

      if (respuesta.success && sentencia.toUpperCase().startsWith('SET DATABASE')) {
        const partes = sentencia.split(/\s+/);
        const nuevaDb = partes[2];
        if (nuevaDb) {
          dbActual = nuevaDb;
          setCurrentDatabase(nuevaDb);
          respuesta.info = `Base de datos activa: ${nuevaDb}`;
        }
      }

      if (respuesta.success && sentencia.toUpperCase().startsWith('DROP DATABASE')) {
        const partes = sentencia.split(/\s+/);
        const dbEliminada = partes[2];
        if (dbEliminada === currentDatabase) {
          setCurrentDatabase('');
          dbActual = '';
        }
      }

      nuevosResultados.push({ sql: sentencia, resultado: respuesta });
    }

    setResultados(prev => [...nuevosResultados, ...prev]);
    setSql('');
    setCargando(false);
  };

  const limpiarResultados = () => setResultados([]);

  const renderizarResultado = (res) => {
    if (!res.resultado.success) {
      return (
        <div className="error-message">
          Error: {res.resultado.error}
          <span className="time-badge">{res.resultado.elapsed_ms?.toFixed(2)} ms</span>
        </div>
      );
    }

    if (res.resultado.type === 'select') {
      return (
        <div>
          <table className="result-table">
            <thead>
              <tr>
                {res.resultado.columns.map(col => <th key={col}>{col}</th>)}
              </tr>
            </thead>
            <tbody>
              {res.resultado.rows.map((fila, idx) => (
                <tr key={idx}>
                  {fila.map((celda, j) => <td key={j}>{celda ?? 'NULL'}</td>)}
                </tr>
              ))}
            </tbody>
          </table>
          <div className="result-meta">
            <span className="time-badge">{res.resultado.elapsed_ms?.toFixed(2)} ms</span>
            <span>{res.resultado.rows.length} filas</span>
          </div>
        </div>
      );
    } else {
      return (
        <div>
          <div className="success-message">
            {res.resultado.message}
            {res.resultado.info && <div className="info-message">{res.resultado.info}</div>}
          </div>
          <div className="result-meta">
            <span className="time-badge">{res.resultado.elapsed_ms?.toFixed(2)} ms</span>
            {res.resultado.affected_rows !== undefined &&
              <span>Filas afectadas: {res.resultado.affected_rows}</span>
            }
          </div>
        </div>
      );
    }
  };

  return (
    <div className="container">
      <h1>TinySQLDb Cliente</h1>

      <div className="config-bar">
        <div className="server-config">
          <label>Servidor URL:</label>
          <input
            type="text"
            value={serverUrl}
            onChange={(e) => setServerUrl(e.target.value)}
            placeholder="http://localhost:8080/query"
            size={35}
          />
        </div>
        <div className="db-context">
          Base de datos activa: <strong>{currentDatabase || '(ninguna)'}</strong>
        </div>
      </div>

      <div className="editor-section">
        <textarea
          rows={10}
          value={sql}
          onChange={(e) => setSql(e.target.value)}
          placeholder="Escribe tus consultas SQL. Separa cada sentencia con punto y coma (;)"
          disabled={cargando}
        />
        <div className="button-group">
          <button onClick={ejecutarConsultas} disabled={cargando}>
            {cargando ? 'Ejecutando...' : 'Ejecutar'}
          </button>
          <button onClick={limpiarResultados} className="clear-button">
            Limpiar resultados
          </button>
        </div>
      </div>

      {resultados.length > 0 && (
        <div className="resultados-section">
          <h2>Resultados</h2>
          {resultados.map((item, idx) => (
            <div key={idx} className="result-item">
              <div className="sql-text">{item.sql}</div>
              <div className="result-content">
                {renderizarResultado(item)}
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

export default App;