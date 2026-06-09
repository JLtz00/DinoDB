# Semana 8 - Split de hojas y busqueda por rango

## Objetivo

Extender el B+ Tree inicial para que pueda crecer mas alla de una sola hoja y
soportar consultas por rango.

## Alcance implementado

- Se agrega `range_scan(start_key, end_key)` como operacion publica del indice.
- Se implementa split de hoja cuando la pagina supera su capacidad.
- Se enlazan hojas mediante el puntero `next`.
- Se crea una raiz interna cuando la raiz hoja se divide.
- Se enruta la busqueda hacia la hoja correcta usando separadores internos.
- Se mantiene la actualizacion de claves existentes sin duplicarlas.

## Diseno

La primera division transforma el arbol de altura 1 en altura 2:

```text
        [ raiz interna ]
          /          \
   [ hoja izq ] -> [ hoja der ]
```

Las busquedas puntuales usan la raiz interna para ubicar la hoja destino. Las
busquedas por rango comienzan en la hoja donde deberia estar la clave inicial y
avanzan por la cadena de hojas hasta superar la clave final.

## Pruebas

Se valida:

- Insercion de suficientes claves para forzar split.
- Busqueda de una clave ubicada despues del split.
- Escaneo por rango entre hojas.
- Incremento de altura del arbol.

## Pendiente

- Split de nodos internos cuando la raiz interna se llena.
- Persistencia de metadatos del indice para reabrir arboles existentes.
