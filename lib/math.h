


// 随机数
XXAPI int xrtRand(int min, int max)
{
	if ( min == max ) {
		return min;
	} else if ( min > max ) {
		return max + (rand() % (min - max + 1));
	} else {
		return min + (rand() % (max - min + 1));
	}
}


