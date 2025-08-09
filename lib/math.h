


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



// 修剪 Double
XXAPI double xrtFixDouble(double x)
{
	if ( x == 0.0 ) {
		return 0.0;
	} else if ( x > 0.0 ) {
		return floor(fabs(x));
	} else {
		return floor(fabs(x)) * -1;
	}
}


