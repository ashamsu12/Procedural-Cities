// Fill out your copyright notice in the Description page of Project Settings.

#include "City.h"
//#include "simplexnoise.h"
//#include "polypartition.h"
#include "HouseBuilder.h"
//#include "ThreadedWorker.h"
struct FPolygon;


std::atomic<unsigned int> AHouseBuilder::workersWorking{ 0 };
std::atomic<unsigned int> AHouseBuilder::housesWorking{ 0 };

AHouseBuilder::AHouseBuilder()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	//SetActorTickEnabled(false);

}

AHouseBuilder::~AHouseBuilder()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	//SetActorTickEnabled(false);
	//if (worker)
	//	delete worker;

}

struct twoInt {
	int32 a;
	int32 b;
};

// very crudely checks which direction an array is going, needs to be checked in this way because it can wrap around 0 unfortunately
bool increasing(std::vector<int> nbrs) {
	for (int i = 1; i < nbrs.size(); i++) {
		if (nbrs[i] == nbrs[i - 1] + 1) {
			return true;
		}
	}
	return false;
}

// similar to above but for a different structure
bool increasing(TArray<twoInt> ints) {
	for (twoInt t : ints) {
		if (t.b == t.a + 1) {
			return true;
		}
	}
	return false;
}

TArray<FMaterialPolygon> getEntrancePolygons(FVector begin, FVector end, float height, float thickness) {
	FMaterialPolygon roof;

	float len = 200;
	FVector dir = getNormal(begin, end, false);
	dir.Normalize();
	roof.type = PolygonType::exteriorSnd;
	roof.width = thickness;
	FVector mainDir = end - begin;
	mainDir.Normalize();
	roof += begin + FVector(0, 0, height) - mainDir * 50;
	roof += end + FVector(0, 0, height) + mainDir * 50;
	roof += end + FVector(0, 0, height) + dir * len + mainDir * 50;
	roof += begin + FVector(0, 0, height) + dir * len - mainDir * 50;


	TArray<FMaterialPolygon> pols;
	pols.Add(roof);
	return pols;
}

// this function returns all of the first grade Room Polygons, these can later be expanded upon when finalizing the room in RoomBuilder
TArray<FRoomPolygon> getInteriorPlanAndPlaceEntrancePolygons(FHousePolygon &f, FPolygon hole, bool ground, float corrWidth, float maxRoomArea, FRandomStream &stream, TArray<FMaterialPolygon> &pols){
	TArray<FLine> lines;
	
	TArray<FRoomPolygon> roomPols;

	FRoomPolygon corners;
	corners.canRefine = false;

	// add corridors from the hole
	TArray<twoInt> connections;
	for (int i = 0; i < hole.points.Num(); i++) {
		roomPols.Add(FRoomPolygon{});
		connections.Add(twoInt{});
	}
	for (int i = 1; i < hole.points.Num() + 1; i++) {
		FVector tangent = hole.points[i%hole.points.Num()] - hole.points[i - 1];
		float midPos = tangent.Size() / 2;
		tangent.Normalize();
		FVector altTangent = FRotator(0, 270, 0).RotateVector(tangent);
		FVector firstAttach = hole.points[i- 1] + (midPos - corrWidth * 0.5) * tangent;
		FVector sndAttach = FVector(0.0f, 0.0f, 0.0f);
		int conn = 0;
		for (int j = 1; j < f.points.Num()+1; j++) {
			FVector res = intersection(firstAttach, firstAttach + altTangent * 100000, f.points[j - 1], f.points[j%f.points.Num()]);
			if (res.X != 0.0f) {
				sndAttach = res;
				conn = j;
				break;
			}

		}
		FVector prevAttach;
		if (sndAttach.X == 0.0f)
			return TArray<FRoomPolygon>();
		if (!ground || !f.entrances.Contains(conn)) {
			corners.points.Add(sndAttach);
		}
		else {
			prevAttach = sndAttach;
		}
		connections[i - 1].b = conn;
		roomPols[i - 1].points.Add(hole.points[i-1]);
		roomPols[i - 1].points.Add(firstAttach);
		roomPols[i - 1].entrances.Add(roomPols[i - 1].points.Num());
		roomPols[i - 1].points.Add(sndAttach);


		firstAttach = hole.points[i-1] + (midPos + corrWidth * 0.5) * tangent;
		sndAttach = FVector(0.0f, 0.0f, 0.0f);
		for (int j = 1; j < f.points.Num()+1; j++) {
			FVector res = intersection(firstAttach, firstAttach + altTangent * 100000, f.points[j - 1], f.points[j%f.points.Num()]);
			if (res.X != 0.0f) {
				sndAttach = res;
				conn = j;
				break;
			}
		}

		if (!ground || !f.entrances.Contains(conn)) {
			if (f.windows.Contains(conn))
				corners.windows.Add(corners.points.Num());
			corners.points.Add(sndAttach);
		}
		else {
			if (stream.FRand() < 0.2 && sndAttach.X != 0.0f && FVector::Dist(prevAttach, sndAttach) < 20000.0f)
				pols.Append(getEntrancePolygons(prevAttach, sndAttach, 450, 50));
		}

		if (i == (hole.points.Num())) {
			TArray<int32> toRemove;
			for (int32 j : roomPols[0].entrances) {
				toRemove.Add(j);
			}
			for (int32 j : toRemove) {
				roomPols[0].entrances.Remove(j);
				// offset bc of the new points we're adding here
				roomPols[0].entrances.Add(j + 3);
			}
			connections[0].a = conn;
			roomPols[0].points.EmplaceAt(0, sndAttach);
			roomPols[0].entrances.Add(1);
			roomPols[0].points.EmplaceAt(1, firstAttach);
			roomPols[0].points.EmplaceAt(2, hole.points[i%hole.points.Num()]);
		}
		else {
			connections[i].a = conn;
			roomPols[i].points.Add(sndAttach);
			roomPols[i].entrances.Add(roomPols[i].points.Num());
			roomPols[i].points.Add(firstAttach);
			roomPols[i].points.Add(hole.points[i%hole.points.Num()]);
		}


	}

	// sew the roomPolygons together by adding walls

	for (int i = 0; i < roomPols.Num(); i++) {
		FRoomPolygon &fp = roomPols[i];

		//if (isIncreasing) {
		fp.exteriorWalls.Add(fp.points.Num());
		for (int j = connections[i].b - 1 == -1 ? f.points.Num(): connections[i].b - 1; j != (connections[i].a - 1 == -1 ? f.points.Num(): connections[i].a - 1); j = j == 0 ? f.points.Num(): j - 1) {
			if (f.windows.Contains(j+1)) {
				fp.windows.Add(fp.points.Num());
			}
			fp.exteriorWalls.Add(fp.points.Num());
			fp.points.Add(f.points[j%f.points.Num()]);
		}
		if (f.windows.Contains(connections[i].a)) {
			roomPols[i].windows.Add(roomPols[i].points.Num());
		}
		fp.exteriorWalls.Add(fp.points.Num());
	}



	TArray<FRoomPolygon> extra;
	// we can split a polygon to the depth of 2 without fearing that any room is left without entrance (proof is omitted as an excercise to the reader)
	for (FRoomPolygon &p : roomPols) {
		if (p.getArea() > maxRoomArea) {
			FRoomPolygon* newP = p.splitAlongMax(0.5, false);
			if (newP) {
				newP->entrances.Add(1);
				p.entrances.Add(1);
				if (newP->getArea() > maxRoomArea) {
					FRoomPolygon* newP2 = newP->splitAlongMax(0.5, false);
					extra.Add(*newP2);
					delete(newP2);
				}
				extra.Add(*newP);
				delete(newP);
				if (p.getArea() > maxRoomArea) {
					FRoomPolygon* newP3 = p.splitAlongMax(0.5, false);
					extra.Add(*newP3);
					delete newP3;
				}
			}
		}
	}
	roomPols.Append(extra);
	


	corners.reverse();
	if (corners.points.Num() > 0) {
		corners.points.Add(FVector(corners.points[0]));
	}
	for (int i = 0; i < corners.points.Num(); i += 2) {
		corners.toIgnore.Add(i);
	}
	for (int i = 1; i < corners.points.Num()+2; i+=2) {
		corners.exteriorWalls.Add(i);
		//corners.windows.Add(i);
	}
	roomPols.Add(corners);

	return roomPols;
}





// this returns polygons corresponding to a hole with the shape "hole" in the polygon f
TArray<FMaterialPolygon> getFloorPolygonsWithHole(FPolygon f, float floorBegin, FPolygon hole) {
	TArray<FMaterialPolygon> polygons;

	FMaterialPolygon f2;
	f2.type = PolygonType::floor;
	f2.points = f.points;


	f2.offset(FVector(0, 0, floorBegin));
	hole.offset(FVector(0, 0, floorBegin));

	f2.normal = FVector(0, 0, -1);
	polygons.Add(f2);


	TArray<FMaterialPolygon> pols;
	TArray<FPolygon> holes;
	holes.Add(hole);
	pols = ARoomBuilder::getSideWithHoles(f2, holes, PolygonType::floor);

	for (auto &pol : pols)
		pol.normal = FVector(0, 0, -1);

	return pols;


}


FPolygon AHouseBuilder::getShaftHolePolygon(FHousePolygon f, FRandomStream stream) {
	FPolygon hole;
	FVector center = f.getCenter(); //f.getRandomPoint(true, 3000, stream);
	//if (center.X == 0.0f)
	//	center = f.getCenter();

	float holeSizeX = 900;
	float holeSizeY = 1200;

	FVector tangent1 = f.points[1] - f.points[0];
	tangent1.Normalize();
	FVector tangent2 = FRotator(0, 90, 0).RotateVector(tangent1);


	hole.points.Add(center + 0.5 * tangent1 * holeSizeX + 0.5 * tangent2 * holeSizeY);
	hole.points.Add(center - 0.5 * tangent1 * holeSizeX + 0.5 * tangent2 * holeSizeY);
	hole.points.Add(center - 0.5 * tangent1 * holeSizeX - 0.5 * tangent2 * holeSizeY);
	hole.points.Add(center + 0.5 * tangent1 * holeSizeX - 0.5 * tangent2 * holeSizeY);



	return hole;
}



// return polygon covering the space that was removed from f
FSimplePlot attemptMoveSideInwards(FHousePolygon &f, int place, FPolygon &centerHole, float len, FVector offset) {
	int prev2 = place > 1 ? place - 2 : place - 2 + f.points.Num();
	FVector dir1 = f.points[prev2] - f.points[place - 1];
	dir1.Normalize();
	FVector dir2 = f.points[(place + 1)%f.points.Num()] - f.points[place%f.points.Num()];
	dir2.Normalize();

	FPolygon line;
	//intersection()
	//FSimplePlot simplePlot;
	FPolygon pol;
	pol.points.Add(f.points[place%f.points.Num()]);
	pol.points.Add(f.points[place - 1]);
	FVector toChange1To = f.points[place - 1] + dir1 * len;
	FVector toChange2To = f.points[place%f.points.Num()] + dir2 * len;
	line.points.Add(toChange1To);
	line.points.Add(toChange2To);
	if (intersection(line, centerHole).X == 0.0f && intersection(line, f).X == 0.0f) {
		f.points[place - 1] = toChange1To;
		f.points[place%f.points.Num()] = toChange2To;
		pol.points.Add(toChange1To);
		pol.points.Add(toChange2To);
		pol.offset(offset);
		f.windows.Add(place);
		FSimplePlot simplePlot;
		simplePlot.obstacles.Append(getBlockingEntrances(f.points, f.entrances, TMap<int32, FVector>(), 400, 1000));
		simplePlot.pol = pol;
		//simplePlot.pol.points.Add(FVector(simplePlot.pol.points[0]));
		simplePlot.pol.reverse();
		simplePlot.type = f.simplePlotType;
		//return pol;
		return simplePlot;
	}
	return FSimplePlot();
}

FSimplePlot attemptRemoveCorner(FHousePolygon &f, int place, FPolygon &centerHole, float len, FVector offset) {
	// remove corner
	FVector p1 = middle(f.points[place - 1], f.points[place%f.points.Num()]);
	FVector p2 = middle(f.points[(place + 1) % f.points.Num()], f.points[place%f.points.Num()]);

	FSimplePlot simplePlot;
	simplePlot.pol.points.Add(p1);
	simplePlot.pol.points.Add(p2);
	simplePlot.pol.points.Add(f.points[place%f.points.Num()]);
	if (intersection(simplePlot.pol, centerHole).X == 0.0f) {
		bool hadW = f.windows.Contains(place);
		bool hadE = f.entrances.Contains(place);

		simplePlot.type = f.simplePlotType; // stream.RandBool() ? SimplePlotType::green : SimplePlotType::asphalt;

		simplePlot.pol.reverse();
		simplePlot.pol.offset(offset);

		f.addPoint(place%f.points.Num(), p1);
		f.addPoint((place + 1) % f.points.Num(), p2);
		f.removePoint((place + 2) %f.points.Num());
		f.windows.Add((place + 1));
		f.entrances.Add((place + 1));
		if (hadW)
			f.windows.Add(place);
		if (hadE)
			f.entrances.Add(place);

		simplePlot.obstacles.Append(getBlockingEntrances(f.points, f.entrances, TMap<int32, FVector>(), 400, 1000));
		return simplePlot;
		//toReturn.Add(simplePlot);
	}
	return FSimplePlot();
}

FSimplePlot attemptTurnSideIntoU(FHousePolygon &f, int place, FPolygon &centerHole, float len, FVector offset, FRandomStream stream) {
	float depth = stream.FRandRange(500, 1500);
	// turn a side inwards into a U
	FVector tangent = f.points[place%f.points.Num()] - f.points[place - 1];
	float lenSide = tangent.Size();
	tangent.Normalize();
	FVector dir = FRotator(0, f.buildLeft ? 90 : 270, 0).RotateVector(tangent);
	FVector first = f.points[place - 1] + (tangent * (lenSide / 3));
	FVector first2 = first + dir * depth;
	FVector snd = f.points[place%f.points.Num()] - (tangent * (lenSide / 3));
	FVector snd2 = snd + dir * depth;

	FSimplePlot simplePlot;
	simplePlot.pol.points.Add(first);
	simplePlot.pol.points.Add(snd);
	simplePlot.pol.points.Add(snd2);
	simplePlot.pol.points.Add(first2);
	simplePlot.pol.normal = FVector(0, 0, -1);
	simplePlot.pol.offset(offset);

	FHousePolygon cp = f;
	if (intersection(simplePlot.pol, centerHole).X == 0.0f && !selfIntersection(simplePlot.pol)) {
		simplePlot.type = f.simplePlotType;


		f.addPoint(place, first);
		f.windows.Add(place);
		f.addPoint(place + 1, first2);
		f.windows.Add(place + 1);

		f.addPoint(place + 2, snd2);
		f.windows.Add(place + 2);
		f.entrances.Add(place + 2);

		f.addPoint(place + 3, snd);
		f.windows.Add(place + 3);
		f.windows.Add(place + 4);

		simplePlot.obstacles.Append(getBlockingEntrances(f.points, f.entrances, TMap<int32, FVector>(), 400, 1000));

		return simplePlot;
	}
	return FSimplePlot();
}



// this method changes the shape of the house to make it less cube-like, can be called several times for more interesting shapes 
void AHouseBuilder::makeInteresting(FHousePolygon &f, TArray<FSimplePlot> &toReturn, FPolygon &centerHole, FRandomStream stream) {
	int place = stream.RandRange(1, f.points.Num());
	float len = stream.FRandRange(400, 1500);
	if (stream.FRand() < 0.2f) {
		// move side inwards
		FSimplePlot res = attemptMoveSideInwards(f, place, centerHole, len, FVector(0,0,30));
		if (res.pol.points.Num() > 0) {
			toReturn.Add(res);
		}
	}

	else if (stream.FRand() < 0.15f && FVector::Dist(f.points[place%f.points.Num()], f.points[place-1]) > 500) {
		FSimplePlot res = attemptRemoveCorner(f, place, centerHole, len, FVector(0, 0, 30));
		if (res.pol.points.Num() > 0) {
			toReturn.Add(res);
		}
	}
	else if (stream.FRand() < 0.1f && FVector::Dist(f.points[place%f.points.Num()],f.points[place - 1]) > 3000) {
		FSimplePlot res = attemptTurnSideIntoU(f, place, centerHole, len, FVector(0, 0, 30), stream);
		if (res.pol.points.Num() > 0) {
			toReturn.Add(res);
		}
	}
}


//TArray<FMaterialPolygon> AHouseBuilder::getShaftSides(FPolygon hole, int openSide, float height) {
//	TArray<FMaterialPolygon> sides;
//	FVector center = hole.getCenter();
//	for (int i = 1; i < hole.points.Num()+1; i++) {
//		if (i == openSide)
//			continue;
//		FMaterialPolygon side;
//		side.type = PolygonType::interior;
//		side.points.Add(hole.points[i - 1]);
//		side.points.Add(hole.points[i - 1] + FVector(0, 0, height));
//		side.points.Add(hole.points[i%hole.points.Num()] + FVector(0, 0, height));
//		side.points.Add(hole.points[i%hole.points.Num()]);
//		sides.Add(side);
//	}
//
//	return sides;
//}


void addStairInfo(FRoomInfo &info, float height, FPolygon &hole) {
	float distFromCorners = 300;
	TArray<FMaterialPolygon> sides;
	for (int i = 1; i < hole.points.Num()+1; i++) {
		FMaterialPolygon side;
		side.points.Add(hole.points[i - 1]);
		side.points.Add(hole.points[i - 1] + FVector(0, 0, height));
		side.points.Add(hole.points[i%hole.points.Num()] + FVector(0, 0, height));
		side.points.Add(hole.points[i%hole.points.Num()]);
		sides.Add(side);
	}
	info.pols.Append(sides);

}

void addFacade(FHousePolygon &f, FRoomInfo &toReturn, float beginHeight, float facadeHeight, float width) {
	for (int i = 1; i < f.points.Num()+1; i++) {
		FMaterialPolygon fac;
		fac.type = PolygonType::exteriorSnd;
		fac.width = width;

		FVector tangent1 = f.points[i - 1] - f.housePosition;
		FVector tangent2 = f.points[i%f.points.Num()] - f.housePosition;
		tangent1.Normalize();
		tangent2.Normalize();

		fac.points.Add(f.points[i - 1] + tangent1 * width + FVector(0, 0, beginHeight));
		fac.points.Add(fac.points[0] + FVector(0, 0, facadeHeight));
		fac.points.Add(f.points[i%f.points.Num()] + tangent2 * width + FVector(0, 0, beginHeight + facadeHeight));
		fac.points.Add(fac.points[2] - FVector(0, 0, facadeHeight));

		toReturn.pols.Add(fac);

	}
}


// recursive method for adding details to part of a roof
void addDetailOnPolygon(int depth, int maxDepth, int maxBoxes, FMaterialPolygon pol, FRoomInfo &toReturn, FRandomStream stream, TMap<FString, UHierarchicalInstancedStaticMeshComponent*> map, TArray<FPolygon> placed, bool canCoverCompletely) {
	if (depth == maxDepth)
		return;
	TArray<FMaterialPolygon> nextShapes;
	if (stream.FRand() < 0.6) {
		// edge detail
		FMaterialPolygon shape = pol;
		shape.reverse();
		float size = stream.FRandRange(50, 500);
		shape.offset(FVector(0, 0, size));
		TArray<FMaterialPolygon> sides = getSidesOfPolygon(shape, PolygonType::exterior, size);
		float width = stream.FRandRange(20, 150);
		for (auto &p : sides)
			p.width = width;

		toReturn.pols.Append(fillOutPolygons(sides));
		toReturn.pols.Append(sides);

	}
	float minHeight = 100;
	float maxHeight = 1000;
	float len = 500;
	float offset = stream.FRandRange(minHeight, maxHeight);
	if (stream.FRand() < 0.5) {
		int numBoxes = stream.RandRange(0, maxBoxes);
		// add box shapes on top of pol
		for (int j = 0; j < numBoxes; j++) {
			offset = stream.FRandRange(minHeight, maxHeight);
			FMaterialPolygon box;
			bool found = true;
			int count = 0;
			do {
				if (count > 4) {
					found = false;
					break;
				}
				box = FMaterialPolygon();
				box.type = PolygonType::exteriorSnd;
				box.normal = FVector(0, 0, -1);
				FVector center = pol.getCenter();
				FVector p1 = center + FVector(stream.FRandRange(0, 4000) * (stream.FRand() < 0.5 ? 1 : -1), stream.FRandRange(0, 4000) * (stream.FRand() < 0.5 ? 1 : -1), offset);
				FVector tangent = pol.points[1] - pol.points[0];
				float firstLen = stream.FRandRange(500, 4000);
				float sndLen = stream.FRandRange(500, 4000);
				tangent.Normalize();
				FVector p2 = p1 + tangent * firstLen;
				tangent = FRotator(0, 90, 0).RotateVector(tangent);
				FVector p3 = p2 + tangent * sndLen;
				tangent = FRotator(0, 90, 0).RotateVector(tangent);
				FVector p4 = p3 + tangent * firstLen;
				box.points.Add(p1);
				box.points.Add(p2);
				box.points.Add(p3);
				box.points.Add(p4);

				count++;

			} while (testCollision(box, placed, 0, pol));//intersection(box, pol).X != 0.0f || !testCollision(box, pol, 0));

			if (found) {
				for (int i = 1; i < box.points.Num()+1; i++) {
					FMaterialPolygon side;
					side.type = PolygonType::exteriorSnd;
					side.points.Add(box.points[i - 1]);
					side.points.Add(box.points[i%box.points.Num()]);
					side.points.Add(box.points[i%box.points.Num()] - FVector(0, 0, offset));
					side.points.Add(box.points[i - 1] - FVector(0, 0, offset));
					toReturn.pols.Add(side);
				}
				placed.Add(box);
				toReturn.pols.Add(box);
				nextShapes.Add(box);
			}
		}
	}
	else if (stream.FRand() < 0.4 && canCoverCompletely) {

		// same shape as pol, but smaller 
		FMaterialPolygon shape = pol;
		shape.offset(FVector(0, 0, offset));
		FVector center = shape.getCenter();
		for (FVector &point : shape.points) {
			FVector tangent = center - point;
			point += tangent / 4;
		}
		if (!pol.getIsClockwise())
			shape.reverse();
		shape.normal = FVector(0, 0, -1);
		//shape.offset(FVector(0, 0, 20));
		for (int i = 1; i < shape.points.Num() + 1; i++) {
			FMaterialPolygon side;
			side.type = PolygonType::roof;
			side.points.Add(shape.points[i - 1]);
			side.points.Add(shape.points[i%shape.points.Num()]);
			side.points.Add(shape.points[i%shape.points.Num()] - FVector(0, 0, offset));
			side.points.Add(shape.points[i - 1] - FVector(0, 0, offset));
			toReturn.pols.Add(side);
		}
		placed.Add(shape);
		toReturn.pols.Add(shape);
		nextShapes.Add(shape);
	}

	else if (stream.FRand() < 0.4 && canCoverCompletely) {
		// pointy shape upwards
		FVector centerP = pol.getCenter();
		centerP += FVector(0, 0, stream.FRandRange(200, 900));
			for (int i = 1; i < pol.points.Num() + 1; i++) {
				FMaterialPolygon rPol;
				rPol.type = PolygonType::roof;
				rPol += pol.points[i - 1];
				rPol += centerP;
				rPol += pol.points[i%pol.points.Num()];
				toReturn.pols.Add(rPol);
				placed.Add(pol);
		}
	}
	
	// try placing meshes
	TArray<FMeshInfo> toPlaceMeshes;
	if (stream.FRand() < 0.45) {
		placeRows(&pol, placed, toPlaceMeshes, FRotator(0, 0, 0), "rooftop_solar", 0.005, 0.005, map, true);
		//for (int i = 0; i < 10; i++)
		//	tempR.attemptPlace(placed, toReturn.meshes, true, 1, "rooftop_solar", FRotator(0, 0, 0), FVector(0, 0, 0), map, false);
	}
	int toKeepStart = stream.RandRange(0, toPlaceMeshes.Num() - 1);
	int toKeepEnd = stream.RandRange(toKeepStart + 1, toKeepStart + 15);
	for (int i = toKeepStart; i < std::min(toKeepEnd, toPlaceMeshes.Num()); i++) {
		toReturn.meshes.Add(toPlaceMeshes[i]);
	}
	toPlaceMeshes.Empty();
	if (stream.FRand() < 0.45) {
		placeRows(&pol, placed, toPlaceMeshes, FRotator(0, 0, 0), "rooftop_ac", 0.007, 0.007, map, true);

		//for (int i = 0; i < 10; i++)
		//	tempR.attemptPlace(placed, toReturn.meshes, true, 1, "rooftop_ac", FRotator(0, 0, 0), FVector(0, 0, 0), map, false);
	}
	toKeepStart = stream.RandRange(0, toPlaceMeshes.Num() - 1);
	toKeepEnd = stream.RandRange(toKeepStart + 1, toKeepStart + 12);
	for (int i = toKeepStart; i < std::min(toKeepEnd, toPlaceMeshes.Num()); i++) {
		toReturn.meshes.Add(toPlaceMeshes[i]);
	}



	for (FMaterialPolygon p : nextShapes)
		addDetailOnPolygon(++depth, maxDepth, 1, p, toReturn, stream, map, TArray<FPolygon>(), true);
}

void addRoofDetail(FMaterialPolygon &roof, FRoomInfo &toReturn, FRandomStream stream, TMap<FString, UHierarchicalInstancedStaticMeshComponent*> map, TArray<FPolygon> placed, bool canCoverCompletely) {

	addDetailOnPolygon(0, 1, 3, roof, toReturn, stream, map, placed, canCoverCompletely);
}


TArray<FMaterialPolygon> potentiallyShrink(FHousePolygon &f, FPolygon &centerHole, FRandomStream stream, FVector offset) {
	TArray<FMaterialPolygon> toReturn;
	FHousePolygon newF = FHousePolygon(f);
	int place = stream.RandRange(1, f.points.Num());
	float len = stream.FRandRange(400, 1500);
	// only shrink one side
	if (stream.FRand() < 0.2) {
		//float modifier = f.windows.Contains(place) && stream.FRand() < 0.1 ? -0.5 : 1;
		FSimplePlot res = attemptMoveSideInwards(f, place, centerHole, len, offset);
		if (res.pol.points.Num() > 2) {
			FMaterialPolygon pol;
			pol.points = res.pol.points;
			pol.type = PolygonType::roof;
			pol.normal = FVector(0, 0, -1);
			//pol.offset(FVector(0, 0, 1));
			toReturn.Add(pol);
			//return toReturn;
		}
	}
	// shrink whole symmetrically
	else if (stream.FRand() < 0.15) {
		float shrinkLen = stream.FRandRange(0.1, 0.3);
		FHousePolygon cp = FHousePolygon(f);
		cp.symmetricShrink(shrinkLen, false);
		if (intersection(cp, centerHole).X == 0.0f && !selfIntersection(cp)) {
			TArray<FPolygon> holes;
			holes.Add(cp);
			TArray<FMaterialPolygon> res = ARoomBuilder::getSideWithHoles(f, holes, PolygonType::roof);
			for (auto &a : res) {
				a.normal = FVector(0, 0, -1);
				a.offset(offset);
				//a.offset(FVector(0, 0, 1));
			}
			toReturn.Append(res);
			f = cp;
			for (int i = 1; i < f.points.Num()+1; i++) {
				f.windows.Add(i);
			}
		}
	}

	// side into u
	else if (stream.FRand() < 0.07) {
		FSimplePlot res = attemptTurnSideIntoU(f, place, centerHole, len, offset, stream);
		if (res.pol.points.Num() > 2) {
			FMaterialPolygon pol;
			pol.points = res.pol.points;
			pol.type = PolygonType::roof;
			pol.normal = FVector(0, 0, -1);
			//pol.offset(FVector(0, 0, 1));
			toReturn.Add(pol);
			//return toReturn;
		}
	}

	// remove corner
	else if (stream.FRand() < 0.15) {
		FSimplePlot res = attemptRemoveCorner(f, place, centerHole, len, offset);
		if (res.pol.points.Num() > 2) {
			FMaterialPolygon pol;
			pol.points = res.pol.points;
			pol.type = PolygonType::roof;
			pol.normal = FVector(0, 0, -1);
			toReturn.Add(pol);
		}
	}
	return toReturn;
}

void AHouseBuilder::buildHouse(bool shellOnly_in) {

	shellOnly = shellOnly_in;
	if (workerWantsToWork) {
		workerWantsToWork = false;
		if (worker)
			delete worker;
		return;
		
	}
	workerWantsToWork = true;
}

void AHouseBuilder::buildHouseFromInfo(FHouseInfo res) {
	//if (procMeshActorClass) {
	if (procMeshActor) {
		procMeshActor->clearMeshes(false);
		for (auto pair : map)
			pair.Value->ClearInstances();
	}
	else {
		procMeshActor = GetWorld()->SpawnActor<AProcMeshActor>(procMeshActorClass, FActorSpawnParameters());
		procMeshActor->init(generationMode);
	}
	//}
	//if (!procMeshActor)
	//	return;
	TArray<FSimplePlot> otherSides;
	//if (firstTime) {
	//	firstTime = false;
		for (FSimplePlot fs : res.remainingPlots) {
			fs.decorate(map);
			for (FMeshInfo mesh : fs.meshes) {
					//if (map.Find(mesh.description))
						map[mesh.description]->AddInstance(mesh.transform);
				}
			}
		res.roomInfo.pols.Append(BaseLibrary::getSimplePlotPolygons(res.remainingPlots));
	//}

	for (FMeshInfo mesh : res.roomInfo.meshes) {
		//if (map.Find(mesh.description))
			map[mesh.description]->AddInstance(mesh.transform);
	}

	procMeshActor->buildPolygons(res.roomInfo.pols, FVector(0, 0, 0));
	//meshesToPlace = res.roomInfo.meshes;
	//isWorking = true;
	//auto *text = GetWorld()->SpawnActor<ATextRenderActor>(ATextRenderActor::StaticClass(), FVector(0.f, 100, 170.f), FRotator(90.f, 180.f, 0.f));
	//text->GetTextRender()->SetText(FText::FromString(TEXT("Dynamic Text")));
	//text->GetTextRender()->SetFont()
	//auto *font = GetWorld()->SpawnActor<UFont>(UFont::StaticClass(), FVector(0.f, 100, 170.f), FRotator(90.f, 180.f, 0.f));
	//font->setf
	//Text->

}


FHouseInfo AHouseBuilder::getHouseInfo()
{
	float dist = FVector::Dist(f[0], f[f.points.Num() - 1]);
	UE_LOG(LogTemp, Warning, TEXT("dist between start and end: %f"), dist);
	FRandomStream stream;
	float corrWidth = 300;
	FHousePolygon pre = f;
	stream.Initialize(f.housePosition.X * 1000 + f.housePosition.Y);
	f.checkOrientation();
	FPolygon hole = getShaftHolePolygon(f, stream);
	FHouseInfo toReturn;
	if (f.canBeModified) {
		for (int i = 0; i < makeInterestingAttempts; i++) {
			makeInteresting(f, toReturn.remainingPlots, hole, stream);
		}
	}
	int floors = f.height;


	float wDens = stream.FRand() * 0.0010 + 0.0005;

	float wWidth = stream.FRand() * 500 + 500;
	float wHeight = stream.FRand() * 400 + 200;

	TArray<FRoomPolygon> roomPols = getInteriorPlanAndPlaceEntrancePolygons(f, hole, true, corrWidth, maxRoomArea, stream, toReturn.roomInfo.pols);

	LivingSpecification liv;
	OfficeSpecification off;
	RestaurantSpecification res;
	StoreSpecification store;

	ApartmentSpecification *spec;
	if (f.type == RoomType::apartment)
		spec = &liv;
	else
		spec = &off;

	// this variable defines how violently the shape of the building will change for each floor, i.e. how often potentiallyShrink is called
	float myChangeIntensity = stream.FRandRange(0, 0.3);

	bool potentialBalcony = f.type == RoomType::apartment && floors < 10 && stream.FRand() < 0.3;
	for (FRoomPolygon &p : roomPols) {
		ApartmentSpecification *toUse = spec;
		p.windowType = WindowType::rectangular;
		if (p.windows.Num() > 0 && f.type == RoomType::apartment) {
			for (int i : p.windows) {
				p.entrances.Add(i);
			}
			if (stream.FRand() < 0.5)
				toUse = &res;
			else
				toUse = &store;
		}
		FRoomInfo newR = toUse->buildApartment(&p, 0, floorHeight, map, false, shellOnly, stream);
		toReturn.roomInfo.pols.Append(newR.pols);
		for (FMeshInfo &mesh : newR.meshes)
			mesh.transform.SetTranslation(mesh.transform.GetTranslation() + FVector(0,0,20));
		toReturn.roomInfo.meshes.Append(newR.meshes);
	}
	FVector rot = getNormal(hole.points[1], hole.points[0], true);
	rot.Normalize();
	FPolygon stairPol = MeshPolygonReference::getStairPolygon(hole.getCenter() + rot*200, rot.Rotation());
	FPolygon elevatorPol = MeshPolygonReference::getStairPolygon(hole.getCenter() - rot * 200, rot.Rotation());
	FVector stairPos = stairPol.getCenter();
	FVector elevatorPos = elevatorPol.getCenter();

	if (!shellOnly) {
		FPolygon corrHeightStair = stairPol;
		corrHeightStair.offset(FVector(0, 0, floorHeight*floors));
		FPolygon corrHeightElevator = elevatorPol;
		corrHeightElevator.offset(FVector(0, 0, floorHeight*floors));
		auto pols = getSidesOfPolygon(corrHeightElevator, PolygonType::interior, floorHeight*floors);
		pols.RemoveAt(1);
		toReturn.roomInfo.pols.Append(pols);
		pols = getSidesOfPolygon(corrHeightStair, PolygonType::interior, floorHeight*floors);
		pols.RemoveAt(1);
		toReturn.roomInfo.pols.Append(pols);
		//toReturn.roomInfo.pols.Append(getShaftSides(elevatorPol, 3, floorHeight * floors));
		//toReturn.roomInfo.pols.Append(getShaftSides(stairPol, 3, floorHeight * floors));

		FMaterialPolygon floor;
		floor.points = f.points;
		floor.offset(FVector(0, 0, 20));
		floor.type = PolygonType::floor;
		toReturn.roomInfo.pols.Add(floor);
	}

	int windowChangeCutoff = stream.RandRange(1, 20);
	WindowType currentWindowType = WindowType(stream.RandRange(0, 3));

	bool roofAccess = stream.FRand() < 0.2;
	bool horizontalFacade = stream.FRand() < 0.15;
	for (int i = 1; i < floors; i++) {
		if (i == windowChangeCutoff)
			currentWindowType = WindowType(stream.RandRange(0,3));

		if (stream.FRand() < myChangeIntensity && f.canBeModified) {
			TArray<FMaterialPolygon> shrinkRes = potentiallyShrink(f, hole, stream, FVector(0, 0, floorHeight*i + 1));
			toReturn.roomInfo.pols.Append(shrinkRes);
		}

		if (!shellOnly) {
			TArray<FMaterialPolygon> floor = getFloorPolygonsWithHole(f, floorHeight*i + 1, stairPol);
			toReturn.roomInfo.pols.Append(floor);
			//toReturn.roomInfo.meshes.Add(FMeshInfo{ "stair", FTransform(rot.Rotation(), stairPos + FVector(0, 0, floorHeight * (i - 1)), FVector(1.0f, 1.0f, 1.0f)) });
		}
		if (horizontalFacade)
			addFacade(f, toReturn.roomInfo, floorHeight*i + 1, 70, 20);

		roomPols = getInteriorPlanAndPlaceEntrancePolygons(f, hole, false, corrWidth, maxRoomArea, stream, toReturn.roomInfo.pols);
		for (FRoomPolygon &p : roomPols) {
			p.windowType = currentWindowType;
			//FRoomInfo newR = ARoomBuilder::buildRoom(&p, 1, floorHeight, map, potentialBalcony, shellOnly, stream);
			FRoomInfo newR = spec->buildApartment(&p, i, floorHeight, map, potentialBalcony, shellOnly, stream);
			newR.offset(FVector(0, 0, floorHeight*i));
			toReturn.roomInfo.pols.Append(newR.pols);
			toReturn.roomInfo.meshes.Append(newR.meshes);
		}
	}
	//int levels = roofAccess ? floors + 1 : floors;
	if (!shellOnly) {
		for (int i = 1; i <= floors; i++) {
			FVector elDir = elevatorPol.points[3] - elevatorPol.points[2];
			elDir.Normalize();
			toReturn.roomInfo.meshes.Add(FMeshInfo{ "elevator", FTransform(rot.Rotation() + FRotator(0, 180, 0), elevatorPos + FVector(0, 0, floorHeight * (i - 1)) + elDir * 180) }); // elevator doors
			FMaterialPolygon above; // space above elevator
			above.type = PolygonType::interior;
			above.points.Add(elevatorPol.points[2] + FVector(0, 0, floorHeight * (i - 1) + 290));
			above.points.Add(elevatorPol.points[3] + FVector(0, 0, floorHeight * (i - 1) + 290));
			above.points.Add(elevatorPol.points[3] + FVector(0, 0, floorHeight * (i - 1) + 400));
			above.points.Add(elevatorPol.points[2] + FVector(0, 0, floorHeight * (i - 1) + 400));
			toReturn.roomInfo.pols.Add(above);

			if (i != floors || roofAccess)
				toReturn.roomInfo.meshes.Add(FMeshInfo{ "stair", FTransform(rot.Rotation(), stairPos + FVector(0, 0, floorHeight * (i - 1)), FVector(1.0f, 1.0f, 1.0f)) });

		}
	}




	FMaterialPolygon roof;
	roof.points = f.points;
	//if (!roof.getIsClockwise())
	//	roof.reverse();
	roof.offset(FVector(0, 0, floorHeight*floors + 1));
	roof.type = PolygonType::roof;
	roof.normal = FVector(0, 0, -1);
	TArray<FPolygon> placed;
	if (generateRoofs) {
		if (roofAccess) {
			auto newRoof = getFloorPolygonsWithHole(f, floorHeight*floors + 1, stairPol);
			for (auto &a : newRoof)
				a.type = PolygonType::roof;
			toReturn.roomInfo.pols.Append(newRoof);
			FMaterialPolygon boxRoof;
			boxRoof.normal = FVector(0, 0, -1);
			boxRoof.type = PolygonType::roof;
			boxRoof.points = hole.points;
			boxRoof.offset(FVector(0, 0, floorHeight*(floors + 1) + 1));
			boxRoof.reverse();
			TArray<FMaterialPolygon> sides = getSidesOfPolygon(boxRoof, PolygonType::exterior, floorHeight);
			FMaterialPolygon &exitSide = sides[0];
			//sides.RemoveAt(0);
			FVector middleP = middle(exitSide[2], exitSide[1]);
			FPolygon entH = getEntranceHole(exitSide[2], exitSide[1], floorHeight, 297, 137, middleP);
			exitSide.points.EmplaceAt(2, entH[2]);
			exitSide.points.EmplaceAt(3, entH[3]);
			exitSide.points.EmplaceAt(4, entH[0]);
			exitSide.points.EmplaceAt(5, entH[1]);

			sides.Add(boxRoof);
			//sides.Add(newP);
			placed.Add(boxRoof);
			toReturn.roomInfo.meshes.Add(getEntranceMesh(exitSide[2], exitSide[1], middleP));
			//toReturn.roomInfo.pols.Append(fillOutPolygons(sides));
			toReturn.roomInfo.pols.Append(sides);
		}
		else
			toReturn.roomInfo.pols.Add(roof);
	}
	if (!shellOnly) {
		TArray<FMaterialPolygon> otherSides = fillOutPolygons(toReturn.roomInfo.pols);
		toReturn.roomInfo.pols.Append(otherSides);
	}

	if (generateRoofs) {
		addRoofDetail(roof, toReturn.roomInfo, stream, map, placed, !roofAccess);
	}
	f = pre;
	return toReturn;
}

// Called when the game starts or when spawned
void AHouseBuilder::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AHouseBuilder::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (workerWantsToWork && workersWorking.load(std::memory_order_relaxed) < maxThreads) {
		workerWantsToWork = false;
		workersWorking++;
		//delete worker;
		worker = new ThreadedWorker(this);
		workerWorking = true;
	}

	if (workerWorking && worker->IsFinished()) {
		buildHouseFromInfo(worker->resultingInfo);
		delete worker;
		workerWorking = false;
		workersWorking--;
		//SetActorTickEnabled(false);

	}

	//if (isWorking) {
	//	int nextStop = std::min(currentIndex + meshesPerTick, meshesToPlace.Num());
	//	for (;currentIndex < nextStop; currentIndex++) {
	//		FMeshInfo curr = meshesToPlace[currentIndex];
	//		map[curr.description]->AddInstance(curr.transform);
	//	}
	//	if (nextStop == meshesToPlace.Num())
	//		isWorking = false;
	//}


}
