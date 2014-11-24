//
//  main.c
//  Proyecto Final
//
//  Created by Gad Levy and Isaac Cherem on 11/20/14.
//  Copyright (c) 2014 Gad Levy and Isaac Cherem. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include <mpi.h>
#include <omp.h>

///send tags
#define INIT 1
#define ENVIAR_PERSONAS_ENTRADA 2
#define ENVIAR_PERSONAS_SALIDA 3
#define CAMBIAR_DIRECCION 4
#define RESPUESTA_DIRECCION 5

/// direcciones
#define DETENIDA 1
#define ENTRADA 2
#define SALIDA 3

///Tiempo
#define PASOS_EN_CINTA 20
#define TIEMPO_PASO 1000
#define REFRESH 1000


#define LAMDA1 90000
#define LAMDA2 80000

typedef struct{
	int direccion; //1 detenida, 2 entrar, 3 salir
	int formadosEntrar;
	int formadosSalir;
	int tiempoParaTerminar;
	int terminaronEntrada;
	int terminaronSalida;
	int seccion;
	int numeroDeCinta;
}Cinta;

int numProcs;
int myId;
Cinta * cintasG;
int sizeG;
MPI_Status status;

void printCintas(Cinta **cintas, int * sizes){
	int i, j;
	for(i=0; i<(numProcs-1); ++i){
		printf("Seccion %i:\n", i+1);
		for(j=0; j<sizes[i]; ++j){
			printf("\tCinta %i: %i\n", j, cintas[i][j].direccion);
		}
	}
}

void contarTiempos(){
	printf("[%i]Contando tiempos...\n", getpid());
	int i;
	while(1){
		for(i=0; i<sizeG; ++i){
			cintasG[i].tiempoParaTerminar--;
		}
		usleep(TIEMPO_PASO);
	}
}

void enviarTerminados(){
	printf("[%i]Enviando terminados...\n", getpid());
	int i, vanAdentro, vanAfuera, temp, tomanVuelo;
	while(1){
		vanAdentro = 0;
		vanAfuera = 0;
		for(i=0; i<sizeG; ++i){
			temp = cintasG[i].terminaronEntrada;//esta instruccion es lo unico que tendria
			cintasG[i].terminaronEntrada -= temp;
			vanAdentro += temp;
			temp = cintasG[i].terminaronSalida;// que ser atomico, es una sola instruccion
			cintasG[i].terminaronSalida -= temp;
			vanAfuera += temp;
		}
		if(vanAfuera == 0 && vanAdentro == 0) continue;
		if(myId==1) printf("Salen por la puerta %i personas\n", vanAfuera);
		else MPI_Send(&vanAfuera, 1, MPI_INT, myId - 1, ENVIAR_PERSONAS_SALIDA ,MPI_COMM_WORLD);
		
		if(vanAdentro == 0) continue;
		
		tomanVuelo = vanAdentro * myId / numProcs;
		printf("Toman vuelo %i personas\n", tomanVuelo);
		vanAdentro -= tomanVuelo;
		if(vanAdentro > 0 && (myId+1) < numProcs)
			MPI_Send(&vanAdentro, 1, MPI_INT, myId + 1, ENVIAR_PERSONAS_ENTRADA ,MPI_COMM_WORLD);
		usleep(REFRESH);
	}
}



//Siempre tienen que haber igual o mayor cantidad de cintas saliendo de mi seccion

//Se pasa gente hacia otra compu con su direccion

//random en cual seccion se sale del sistema cada persona  1/(seccionesFaltantes+1)

//cuando va a cambiar el sentido de la cinta checa que la suma de las cintas hacia donde 
//va a ir no haya igual de entradas que de salidas

int poisson1(){
  int n = 0; //counter of iteration
  double limit; 
  double x;  //pseudo random number
  limit = exp(-LAMDA1);
  x = rand() / INT_MAX; 
  while (x > limit) {
	n++;
	x *= rand() / INT_MAX;
  }
  n = rand() % LAMDA1;
  return n;
}
int poisson2(){
  int n = 0; //counter of iteration
  double limit; 
  double x;  //pseudo random number
  limit = exp(-LAMDA2);
  x = rand() / INT_MAX; 
  while (x > limit) {
	n++;
	x *= rand() / INT_MAX;
  }
  n = rand() % LAMDA2;
  return n;
}

void tratarCambiar(Cinta *c, int direccion){
	MPI_Status status;
	int arreglo[3], nuevaDireccion;
	arreglo[0] = c->numeroDeCinta;
	arreglo[1] = c->seccion;
	arreglo[2] = direccion;
	
	MPI_Send(arreglo, 3, MPI_INT, 0, CAMBIAR_DIRECCION ,MPI_COMM_WORLD);
	
	//creo que aqui es donde explota
	MPI_Recv(&nuevaDireccion, 1 , MPI_INT, 0, RESPUESTA_DIRECCION, MPI_COMM_WORLD, &status);
	
	c->direccion=nuevaDireccion;
}

void recibirPersonas(int numPersonas, int direccion){
	printf("[%i]Recibiendo %i personas en la seccion %i con direccion %i\n",
		getpid(), numPersonas, myId, direccion);
	int cintaActual = 0;
	if(direccion==ENTRADA){
		//Si hay gente formada de un lado, no dejamos que se formen del otro lado para evitar el problema de inaicion
		while(1){
			Cinta *c=&cintasG[cintaActual];
			if(c->formadosSalir==0){
				c->formadosEntrar++;
				numPersonas--;
			}
			cintaActual = (cintaActual + 1) % sizeG;
			if(numPersonas==0){
				break;
			}
		}
	}
	
	if(direccion==SALIDA){
		while(1){
			Cinta *c=&cintasG[cintaActual];
			if(c->formadosSalir==0){
				c->formadosEntrar++;
				numPersonas--;
			}	
			cintaActual = (cintaActual + 1) % sizeG;
			if(numPersonas==0){
				break;
			}
		}
		
	}
	printf("[%i]Termine de formar personas en la seccion %i con direccion %i\n",
		getpid(), myId, direccion);
}


void cinta(Cinta *c){
	printf("[%i]Cinta %i seccion %i funcionando...\n", getpid(), c->numeroDeCinta, c->seccion);
	while(1){
		usleep(REFRESH);
		if(c->formadosEntrar > 0 || c-> formadosSalir > 0)
			printf("[seccion:%i cinta:%i direccion:%i] formados entrada:%i salida:%i\n",
				c->seccion, c->numeroDeCinta, c->direccion, c->formadosEntrar, c->formadosSalir);
		if(c->direccion==ENTRADA){
			if(c->formadosEntrar>0){
				c->formadosEntrar--;
				c->tiempoParaTerminar=PASOS_EN_CINTA;
				c->terminaronEntrada++;
			}
			else{
				if(c->tiempoParaTerminar <= 0){
					if(c->formadosSalir>0){
						tratarCambiar(c, SALIDA);
					}
					else{
						tratarCambiar(c, DETENIDA);
					}
				}
			}
		}
		else if(c->direccion==SALIDA){
			if(c->formadosSalir>0){
				c->formadosSalir--;
				c->tiempoParaTerminar=PASOS_EN_CINTA;
				c->terminaronSalida++;
			}
			else{
				if(c->tiempoParaTerminar <= 0){
					if(c->formadosEntrar>0){
						tratarCambiar(c, ENTRADA);
					}
					else{
						tratarCambiar(c, DETENIDA);
					}
				}
			}
		}
		else if(c->direccion==DETENIDA){
			if(c->formadosEntrar == 0 && c->formadosSalir == 0) continue;
			if(c->formadosEntrar>0 && c->formadosSalir==0){
				tratarCambiar(c,ENTRADA);
			}
			if(c->formadosSalir>0 && c->formadosEntrar==0){
				tratarCambiar(c,SALIDA);
			}
			if(c->formadosEntrar>0 && c->formadosSalir>0){
				if(rand()%2){
					tratarCambiar(c,SALIDA);
					//volver a formar los que van hacia el otro lado
					recibirPersonas(c->formadosEntrar, ENTRADA);
					c->formadosEntrar=0;
				}else{
					tratarCambiar(c,ENTRADA);
					//volver a formar los que van hacia el otro lado
					recibirPersonas(c->formadosSalir, SALIDA);
					c->formadosSalir=0;
				}
			}
		}
	}
}

void escucharRecibidos(){
	printf("[%i]Escuachando recibidos...\n", getpid());
	int personas;
	MPI_Status status;
	
	#pragma omp parallel num_threads(2)
	if(omp_get_thread_num() == 0 && myId < numProcs){//solo si no es el de la orilla escucha que le manden desde adentro
		while(1){
			MPI_Recv(&personas, 1, MPI_INT, MPI_ANY_SOURCE, ENVIAR_PERSONAS_SALIDA, MPI_COMM_WORLD, &status);
			recibirPersonas(personas, SALIDA);
			usleep(REFRESH);
		}
	}
	if(omp_get_thread_num() == 1){
		while(1){
			MPI_Recv(&personas, 1, MPI_INT, MPI_ANY_SOURCE, ENVIAR_PERSONAS_ENTRADA, MPI_COMM_WORLD, &status);
			recibirPersonas(personas, ENTRADA);
			usleep(REFRESH);
		}
	}
}

escucharCambios(Cinta ** cintas, int * nCintas){
	printf("[%i]Escuchando cambios...\n", getpid());
	printCintas(cintas, nCintas);
	
	int arreglo[3];//num cinta, seccion, dir deseada
	int i, mismaDireccion, direccionContraria;
	
	while(1){
		usleep(REFRESH);
		MPI_Recv(arreglo, 3, MPI_INT, MPI_ANY_SOURCE, CAMBIAR_DIRECCION, MPI_COMM_WORLD, &status);
		
		arreglo[1] -= 1;
		
		for(i=0; i<nCintas[arreglo[1]]; ++i){
			Cinta c=cintas[arreglo[1]][i];
			if(cintas[arreglo[1]][i].direccion == arreglo[2]) mismaDireccion++;
		}
		
		if(arreglo[2]==ENTRADA)
		{
			if(arreglo[1] >= numProcs){
				//si esta en la orilla no hay cuello de boteya asi que se cambia
				cintas[arreglo[1]][arreglo[0]].direccion = ENTRADA;
			} 
			else
			for(i=0; i<nCintas[arreglo[1] + 1]; ++i){
			
				Cinta c=cintas[arreglo[1] + 1][i];
				if(cintas[arreglo[1] + 1][i].direccion != arreglo[2]) direccionContraria++;
			}
		}
		if(arreglo[2]==SALIDA){
			if(arreglo[1] == 1){
				//si esta en la orilla no hay cuello de boteya asi que se cambia
				cintas[arreglo[1]][arreglo[0]].direccion = SALIDA;
			} 
			else
			for(i=0; i<nCintas[arreglo[1] - 1]; ++i){
			
				Cinta c=cintas[arreglo[1] - 1][i];
				if(cintas[arreglo[1] - 1][i].direccion != arreglo[2]) direccionContraria++;
			}
		}
		
		int nuevaDireccion; 
		if(direccionContraria > mismaDireccion || arreglo[2] == 1){//si se quiere apagar siempre la dejas
			nuevaDireccion = arreglo[2];
			cintas[arreglo[1]][arreglo[0]].direccion = arreglo[2];
			printf("La cinta %i de la seccion %i ahora va hacia %i\n", arreglo[0], arreglo[1] + 1, nuevaDireccion);
		}else{
			nuevaDireccion = cintas[arreglo[1]][arreglo[0]].direccion;
			printf("La cinta %i de la seccion %i se queda llendo hacia %i\n", arreglo[0], arreglo[1] + 1, nuevaDireccion);
		}
		printCintas(cintas, nCintas);
		
		MPI_Send(&nuevaDireccion, 1 , MPI_INT, arreglo[1], RESPUESTA_DIRECCION ,MPI_COMM_WORLD);
	}
}

generarPersonasSalida(){
	printf("[%i]Generando personas de salida...\n", getpid());
	int personas, seccion;
	while(1){
		usleep(poisson2());
		
		personas=rand()%30 + 10;
		seccion = rand()%(numProcs - 1) + 1;
		printf("Llega un vuelo en la seccion %i con %i personas\n", seccion, personas);
		MPI_Send(&personas, 1, MPI_INT, seccion, ENVIAR_PERSONAS_SALIDA ,MPI_COMM_WORLD);
	}
}

generarPersonasEntrada(){
	printf("[%i]Generando personas de entrada...\n", getpid());
	int random,personas;
	
	while(1){
		usleep(poisson1());
		personas=rand()%30+10;
		printf("[%i]Llegan por la entrada %i personas\n", getpid(), personas);
		MPI_Send(&personas, 1, MPI_INT, 1, ENVIAR_PERSONAS_ENTRADA ,MPI_COMM_WORLD);
	}
}

int main(int argc, char **argv){
	int i, j;
	int buf;

	MPI_Init(&argc,&argv);
	MPI_Status status;
	MPI_Comm_size(MPI_COMM_WORLD,&numProcs);
	MPI_Comm_rank(MPI_COMM_WORLD,&myId);

	int nCintas[numProcs];
	
	srand(time(NULL));
	if(myId==0){
		
		Cinta * cintas[numProcs];
		for (i=0; i<numProcs; i++) {
			int random=rand()%4 + 2;
			nCintas[i] = random;
			cintas[i] = malloc(sizeof(Cinta)*random);
		}
		
		for (i=0; i<numProcs; i++) {
			for(j=0; j<nCintas[i]; ++j){
				cintas[i][j].direccion=DETENIDA;
				cintas[i][j].formadosEntrar=0;
				cintas[i][j].formadosSalir=0;
				cintas[i][j].seccion=i;
				cintas[i][j].tiempoParaTerminar=0;
				cintas[i][j].numeroDeCinta=j;
			}
		}
		
		MPI_Scatter(nCintas, 1, MPI_INT, &buf, 1, MPI_INT, 0, MPI_COMM_WORLD); 		
		
		#pragma omp parallel num_threads(3)
		{switch(omp_get_thread_num()){
			case 0: generarPersonasEntrada(); break;
			case 1: escucharCambios(cintas, nCintas); break;
			case 2: generarPersonasSalida();
		}}
		
	}
	else{
		int buf;

		MPI_Scatter(nCintas, 1, MPI_INT, &buf, 1, MPI_INT, 0, MPI_COMM_WORLD);
		
		sizeG = buf;

		cintasG = malloc(sizeof(Cinta)*sizeG);
		
		for(i=0; i<sizeG; ++i){
			cintasG[i].direccion=DETENIDA;
			cintasG[i].formadosEntrar=0;
			cintasG[i].formadosSalir=0;
			cintasG[i].seccion=myId;
			cintasG[i].tiempoParaTerminar=0;
			cintasG[i].numeroDeCinta=i;
		}
		
		#pragma omp parallel num_threads(buf+3)
		{switch(omp_get_thread_num()){
			case 0: contarTiempos(); break;
			case 1: enviarTerminados(); break;
			case 2: escucharRecibidos(); break;
			default: cinta(cintasG+omp_get_thread_num()-3);
		}}
	
		
	}
	
	free(cintasG);
	MPI_Finalize();
}
