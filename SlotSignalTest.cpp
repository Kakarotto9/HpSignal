#include"SlotSignal.h"

using namespace std;

void hello()
{
	int a = 0;
	int b = 1;
	cout << "hello" << endl;

}
void print(int i)
{
	cout << "print " << i << endl;
}
void test()
{
	{
		Signal<void(void)> sig;
		Slot Slot1 = sig.connect(&hello);
		sig.call();
	}
	Signal<void(int)> sig1;
	Slot Slot1 = sig1.connect(&print);
	Slot Slot2 = sig1.connect(std::bind(&print, std::placeholders::_1));
	std::function<void(int)> func1(std::bind(&print, std::placeholders::_1));
	Slot Slot3 = sig1.connect(std::move(func1));
	{
		Slot Slot4 = sig1.connect(std::bind(&print, 666));
		sig1.call(4);
	}
	sig1.call(4);
}

int main()
{
	test();
	char c; cin >> c;

}
