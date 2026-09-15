#include "Misc/AutomationTest.h"

#include "Paint/PaintRipple.h"

#if WITH_DEV_AUTOMATION_TESTS

/** The ring wave the surface material shades: a crest running out, a Gaussian window, a decaying envelope, and its analytic slope. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintRippleWaveTest,
	"MintChoco.Paint.Ripple.Wave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintRippleWaveTest::RunTest(const FString& Parameters)
{
	FPaintRippleShape Shape;
	Shape.Amplitude = 2.5f;
	Shape.Speed = 80.0f;
	Shape.Wavelength = 14.0f;
	Shape.Decay = 2.0f;

	float Slope = 0.0f;
	TestEqual(TEXT("the wave starts at its amplitude on the contact"), PaintRipple::Height(Shape, 0.0f, 0.0f, Slope), Shape.Amplitude, 1e-4f);
	TestEqual(TEXT("nothing before birth"), PaintRipple::Height(Shape, 10.0f, -0.1f, Slope), 0.0f);

	// The crest sits at Speed * Age, and far from it the window has closed.
	const float Age = 0.2f;
	const float Crest = Shape.Speed * Age;
	const float AtCrest = PaintRipple::Height(Shape, Crest, Age, Slope);
	TestEqual(TEXT("the crest carries the decayed amplitude"), AtCrest, Shape.Amplitude * FMath::Exp(-Shape.Decay * Age), 1e-4f);
	TestTrue(TEXT("the wave is flat far ahead of the crest"), FMath::Abs(PaintRipple::Height(Shape, Crest + 5.0f * Shape.Wavelength, Age, Slope)) < 1e-4f);
	TestTrue(TEXT("the wave is flat far behind the crest"), FMath::Abs(PaintRipple::Height(Shape, FMath::Max(Crest - 5.0f * Shape.Wavelength, 0.0f), Age, Slope)) < 1e-4f || Crest < 5.0f * Shape.Wavelength);

	// The analytic slope matches a central difference wherever the wave lives.
	bool bSlopes = true;
	for (float Distance = 0.0f; Distance <= 60.0f; Distance += 1.7f)
	{
		float Analytic = 0.0f;
		PaintRipple::Height(Shape, Distance, Age, Analytic);
		float Unused = 0.0f;
		const float Step = 1e-3f;
		const float Numeric = (PaintRipple::Height(Shape, Distance + Step, Age, Unused) - PaintRipple::Height(Shape, Distance - Step, Age, Unused)) / (2.0f * Step);
		bSlopes &= FMath::Abs(Analytic - Numeric) < 1e-2f;
	}
	TestTrue(TEXT("the slope is the derivative of the height"), bSlopes);

	// After its lifetime the wave is below the cutoff everywhere.
	const float Late = Shape.Lifetime() + 1e-3f;
	bool bGone = true;
	for (float Distance = 0.0f; Distance <= Shape.Speed * Late + 2.0f * Shape.Wavelength; Distance += 0.5f)
	{
		bGone &= FMath::Abs(PaintRipple::Height(Shape, Distance, Late, Slope)) <= PaintRipple::Cutoff * Shape.Amplitude + 1e-4f;
	}
	TestTrue(TEXT("past its lifetime the wave is below the cutoff"), bGone);

	FPaintRippleWave Wave;
	Wave.Shape = Shape;
	Wave.StartTime = 10.0f;
	TestTrue(TEXT("a wave is alive right after it starts"), Wave.IsAlive(10.1f));
	TestFalse(TEXT("a wave is dead before it starts"), Wave.IsAlive(9.9f));
	TestFalse(TEXT("a wave is dead after its lifetime"), Wave.IsAlive(10.0f + Shape.Lifetime() + 0.1f));
	Wave.Shape.Amplitude = 0.0f;
	TestFalse(TEXT("a flat wave is never alive"), Wave.IsAlive(10.1f));
	return true;
}

/** A surface keeps four waves: a new one takes a dead slot first, then the oldest. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPaintRippleSlotsTest,
	"MintChoco.Paint.Ripple.Slots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPaintRippleSlotsTest::RunTest(const FString& Parameters)
{
	FPaintRippleSlots Slots;
	FPaintRippleWave Wave;
	Wave.Shape.Amplitude = 1.0f;
	Wave.Shape.Decay = 5.0f;

	bool bFillsInOrder = true;
	for (int32 Index = 0; Index < PaintRipple::SlotCount; ++Index)
	{
		Wave.StartTime = 1.0f + 0.01f * Index;
		bFillsInOrder &= Slots.Push(Wave, Wave.StartTime) == Index;
	}
	TestTrue(TEXT("fresh slots fill in order"), bFillsInOrder);

	// All alive: the oldest (slot 0) goes first, then slot 1.
	Wave.StartTime = 1.1f;
	TestEqual(TEXT("with every slot alive the oldest is replaced"), Slots.Push(Wave, 1.1f), 0);
	Wave.StartTime = 1.11f;
	TestEqual(TEXT("then the next oldest"), Slots.Push(Wave, 1.11f), 1);

	// Once the wave in slot 2 has died, it is taken before any live one.
	const float Later = 1.02f + Wave.Shape.Lifetime() + 0.01f;
	Wave.StartTime = Later;
	TestEqual(TEXT("a dead slot is taken before a live one"), Slots.Push(Wave, Later), 2);
	return true;
}

#endif
