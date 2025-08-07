


// 随机数
XXAPI int xrtRand(int min, int max)
{
	if ( min == max ) {
		return min;
	} else if ( min > max ) {
		return max + (rand() % (min - max));
	} else {
		return min + (rand() % (max - min));
	}
}


