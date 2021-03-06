//#define _SCL_SECURE_NO_WARNINGS
//#pragma warning(disable : 4996)

#include "stdafx.h"
#include <stdlib.h>
#include <iostream>
#include <algorithm>    // std::sort
#include <mpi.h>


// -----------------------------------------------
// BEGIN OF SETTINGS AREA

// Debug level of output. Possible values: 0 - 5.
// Use with caution, 5th debug level will print the whole array 2 times.
const int DEBUG_LEVEL = 1;

// Function that will return array of values for sorting
int arrayInput(int * arr) {
	int arr_size = 1024;
	arr = new int[arr_size];
	for (int i = 0; i < arr_size; i++) {
		arr[i] = rand() % 1000;
	}
	return arr_size;
}

// Function that will output array
void arrayOutput(int * arr, int arr_size) {
	if (DEBUG_LEVEL >= 5) {
		std::cout << "Array: ";
		for (int i = 0; i < arr_size; i++) {
			std::cout << arr[i] << " ";
		}
		std::cout << std::endl;
	}

	bool sorted = true;
	//sorting check
	for (int i = 1; i < arr_size; i++) {
		if (arr[i] < arr[i - 1]) sorted = false;
	}
	std::cout << std::endl << "Array sorted: " << sorted << std::endl;
}

// END OF SETTINGS AREA
// -----------------------------------------------


int SIZE;
int RANK;
MPI_Status STATUS;

bool inline isPowerOfTwo(int n) {
	return (n & (n - 1)) == 0;
}

int getRandomElement(int * arr, int arr_size) {
	int rnd = int(((double)(rand()) / RAND_MAX) * (arr_size));
	return arr[rnd];
}

int partition_with_pivot(int * arr, int arr_size, int pivot)
{
	int i = 0;
	int j = arr_size - 1;

	while (i <= j)
	{
		while (arr[i] < pivot) i++;
		while (arr[j] > pivot) j--;

		if (i <= j)
		{
			std::swap(arr[i], arr[j]);
			i++;
			j--;
		}
	}
	return i;
}

int main(int argc, char **argv)
{
	int * arr = new int[0];
	int arr_size;
	int global_array_size;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &SIZE);
	MPI_Comm_rank(MPI_COMM_WORLD, &RANK);

	//sanity check
	if (SIZE < 2 || !isPowerOfTwo(SIZE)) {
		std::cout << "Wrong number of processes (< 1 or not power of two)" << std::endl;
		MPI_Finalize();
		return -1;
	}
	
	//0 поток всем распределяет данные
	if (RANK == 0) {
		global_array_size = arrayInput(arr);
		arr_size = global_array_size / SIZE;
		for (int i = 1; i < SIZE; i++)
		{
			MPI_Ssend(&arr_size, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
			MPI_Ssend(&(arr[i * arr_size]), arr_size, MPI_INT, i, 0, MPI_COMM_WORLD);
		}
		int * tarr = new int[arr_size];
		std::copy(arr, arr + arr_size, tarr);
		arr = tarr;
	}
	else {
		MPI_Recv(&arr_size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &STATUS);
		arr = new int[arr_size];
		MPI_Recv(arr, arr_size, MPI_INT, 0, 0, MPI_COMM_WORLD, &STATUS);
	}

	//делаем наше решето
	for (int group_size = SIZE; group_size > 1; group_size /= 2) {
		int pivot = 0;
		
		MPI_Barrier(MPI_COMM_WORLD);
		//1. Определяется центральный элемент для группы
		if (DEBUG_LEVEL >= 1) std::cout << "RANK: " << RANK << ", stage 1" << std::endl;
		#pragma region BaseElement
		//определяем базовый поток
		if (RANK % group_size == 0) {
			pivot = getRandomElement(arr, arr_size);
			int temp = 0;

			for (int i = 1; i < group_size; i++) {
				MPI_Recv(&temp, 1, MPI_INT, RANK + i, 0, MPI_COMM_WORLD, &STATUS);
				pivot += temp;
			}
			pivot /= group_size;

			for (int i = 1; i < group_size; i++) {
				MPI_Ssend(&pivot, 1, MPI_INT, RANK + i, 0, MPI_COMM_WORLD);
			}
		}
		else {
			//присылают базе
			int temp = getRandomElement(arr, arr_size);
			MPI_Ssend(&temp, 1, MPI_INT, RANK - (RANK % group_size), 0, MPI_COMM_WORLD);

			//получают от базы
			MPI_Recv(&pivot, 1, MPI_INT, RANK - (RANK % group_size), 0, MPI_COMM_WORLD, &STATUS);
		}
		#pragma endregion

		//2. В процессе раскидываются числа относительно центрального элемента
		if (DEBUG_LEVEL >= 1) std::cout << "RANK: " << RANK << ", stage 2" << std::endl;
		int middle = partition_with_pivot(arr, arr_size, pivot);

		//3. Правая половина потоков группы отдает меньшие элементы левой
		if (DEBUG_LEVEL >= 1) std::cout << "RANK: " << RANK << ", stage 3" << std::endl;
		int * leftArr = new int[0];
		int leftArrSize;

		int * rightArr = new int[0];
		int rightArrSize;

		#pragma region SendLower
		//определяем, кто шлет
		if (((RANK / (group_size / 2)) % 2) == 1) {
			//посылающие
			if (DEBUG_LEVEL >= 1) std::cout << "Rank: " << RANK << ", SENDING LOWER VALUES" << std::endl;
			
			int leftSize = middle;

			if (DEBUG_LEVEL >= 2) std::cout << "Rank: " << RANK << ", sent leftSize = " << leftSize << std::endl;
			MPI_Ssend(&leftSize, 1, MPI_INT, RANK - group_size / 2, 0, MPI_COMM_WORLD);
			if (leftSize > 0) {
				MPI_Ssend(arr, leftSize, MPI_INT, RANK - group_size / 2, 0, MPI_COMM_WORLD);
			}
		}
		else {
			//принимающие
			if (DEBUG_LEVEL >= 1) std::cout << "Rank: " << RANK << ", RECEIVING LOWER VALUES" << std::endl;
			
			MPI_Recv(&leftArrSize, 1, MPI_INT, RANK + group_size / 2, 0, MPI_COMM_WORLD, &STATUS);

			if (DEBUG_LEVEL >= 2) std::cout << "Rank: " << RANK << ", received leftSize = " << leftArrSize << std::endl;

			if (leftArrSize > 0) {
				delete[] leftArr;
				leftArr = new int[leftArrSize];
				MPI_Recv(leftArr, leftArrSize, MPI_INT, RANK + group_size / 2, 0, MPI_COMM_WORLD, &STATUS);
			}
		}
		#pragma endregion

		//4. Левая половина потоков группы отдает большие элементы правой и собирает в новый arr
		if (DEBUG_LEVEL >= 1) std::cout << "RANK: " << RANK << ", stage 4" << std::endl;
		#pragma region SendHigher
		if (((RANK / (group_size / 2)) % 2) == 0) {
			//посылающие
			if (DEBUG_LEVEL >= 1) std::cout << "Rank: " << RANK << ", SENDING HIGHER VALUES" << std::endl;

			int rightSize = arr_size - middle;

			if (DEBUG_LEVEL >= 2) std::cout << "Rank: " << RANK << ", sent rightSize = " << rightSize << std::endl;

			MPI_Ssend(&rightSize, 1, MPI_INT, RANK + group_size / 2, 0, MPI_COMM_WORLD);
			if (rightSize > 0) {
				MPI_Ssend(arr + middle, rightSize, MPI_INT, RANK + group_size / 2, 0, MPI_COMM_WORLD);
			}

			if (DEBUG_LEVEL >= 1) std::cout << "Rank: " << RANK << ", leftArrSize = " << leftArrSize << std::endl;
			arr_size = middle + leftArrSize;
			int * temp_arr = new int[arr_size];
			std::copy(arr, arr + middle, temp_arr);
			if (leftArrSize > 0) std::copy(leftArr, leftArr + leftArrSize, temp_arr + middle);

			delete[] arr;
			arr = temp_arr;
		}
		else {
			//принимающие
			if (DEBUG_LEVEL >= 1) std::cout << "Rank: " << RANK << ", RECEIVING HIGHER VALUES" << std::endl;

			MPI_Recv(&rightArrSize, 1, MPI_INT, RANK - group_size / 2, 0, MPI_COMM_WORLD, &STATUS);

			if (DEBUG_LEVEL >= 2) std::cout << "Rank: " << RANK << ", received rightArrSize = " << rightArrSize << std::endl;

			if (rightArrSize > 0) {
				delete[] rightArr;
				rightArr = new int[rightArrSize];
				MPI_Recv(rightArr, rightArrSize, MPI_INT, RANK - group_size / 2, 0, MPI_COMM_WORLD, &STATUS);
			}

			if (DEBUG_LEVEL >= 1) std::cout << "Rank: " << RANK << ", Array received and copied, size = " << rightArrSize << std::endl;

			int old_arr_size = arr_size;
			arr_size = arr_size - middle + rightArrSize;
			int * temp_arr = new int[arr_size];

			if (DEBUG_LEVEL >= 2) std::cout << "Rank: " << RANK << ", temp arr created, size: " << arr_size << std::endl;
			if (rightArrSize > 0) std::copy(rightArr, rightArr + rightArrSize, temp_arr);
			if (DEBUG_LEVEL >= 2) std::cout << "Rank: " << RANK << ", right array copied to temp_arr" << std::endl;
			std::copy(arr + middle, arr + old_arr_size, temp_arr + rightArrSize);
			if (DEBUG_LEVEL >= 2) std::cout << "Rank: " << RANK << ", arr copied to temp_arr" << std::endl;

			delete[] arr;
			arr = temp_arr;
			if (DEBUG_LEVEL >= 2) std::cout << "Rank: " << RANK << ", arr pointer changed" << std::endl;
		}
		#pragma endregion


		//5. Группы уменьшаются в 2 раза
		if (DEBUG_LEVEL >= 1) std::cout << "Rank: " << RANK << ", stage 5" << std::endl;
		MPI_Barrier(MPI_COMM_WORLD);
		if (DEBUG_LEVEL >= 1) {
			if (RANK == 0) std::cout << "group_size " << group_size << " ended, next iteration." << std::endl;
			MPI_Barrier(MPI_COMM_WORLD);
		}
	}


	//сортируем данные внутри потоков
	if (DEBUG_LEVEL >= 1) if (RANK == 0) std::cout << std::endl << "Local sorting began" << std::endl;
	std::sort(arr, arr + arr_size);
	if (DEBUG_LEVEL >= 5) {
		std::cout << "Rank: " << RANK << ": ";
		for (int i = 0; i < arr_size; i++) {
			std::cout << arr[i] << " ";
		}
		std::cout << std::endl;
	}


	//сливаем все в 0 поток
	if (DEBUG_LEVEL >= 1) if (RANK == 0) std::cout << std::endl << "Merging began" << std::endl;
	if (RANK == 0) {
		int * result_arr = new int[global_array_size];
		std::copy(arr, arr + arr_size, result_arr);

		int local_counter = 0;
		int global_counter = arr_size;
		for (int i = 1; i < SIZE; i++) {
			MPI_Recv(&local_counter, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &STATUS);
			if (local_counter > 0) {
				int * tArr = new int[local_counter];
				MPI_Recv(tArr, local_counter, MPI_INT, i, 0, MPI_COMM_WORLD, &STATUS);
				std::copy(tArr, tArr + local_counter, result_arr + global_counter);
				global_counter += local_counter;
				delete[] tArr;
			}
		}
		arr_size = global_counter;
		arr = result_arr;
	}
	else {
		MPI_Ssend(&arr_size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
		if (arr_size > 0) {
			MPI_Ssend(arr, arr_size, MPI_INT, 0, 0, MPI_COMM_WORLD);
		}
	}


	MPI_Finalize();
	if (RANK == 0) {
		arrayOutput(arr, arr_size);
	}

	delete[] arr;

	if (DEBUG_LEVEL >= 1) std::cout << "Process " << RANK << " successfully finished the work" << std::endl;
	return 0;
}
