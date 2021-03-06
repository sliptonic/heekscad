// sketchinterface.cpp
// Copyright (c) 2009, Dan Heeks
// This program is released under the BSD license. See the file COPYING for details.

#include "stdafx.h"
#include "../sketchsolve/src/solve.h"
#include "OCCSolver.h"

//This file is used to deal with the numeric solvers trouble with pointOnPoint constraints.
//They seem to cause serious slowness and instability in the solver. Since they are trivial
//to solve and are used liberally in heekscad. It was decided to maintain the appearance
//that they can be solved efficiently by writing an algorithm that converts to and from
//a point free and pointed data structure. 

//This is an n log n realization of that scheme.

OCCSolver solve;

std::vector<double*> usedparms;
std::set<double*> hasusedparms;
std::set<double*> oldparms;
std::vector<double> parmdata;
std::vector<double*> newparms;
std::map<double*,double*> parmmap;
std::map<double*,std::list<double*> > rparmmap;
std::vector<constraint> newcons;
std::vector<constraint> arccons;

double* mapdouble(double* v)
{
	if(v)
	{
		if(parmmap[v])
			v = parmmap[v];
		else
			if(oldparms.find(v) != oldparms.end())
				newparms.push_back(v);
			else
				return v;

		if(hasusedparms.find(v) == hasusedparms.end())
		{
			usedparms.push_back(v);
			hasusedparms.insert(v);
		}
	}
	return v;
}

point mappoint(point p)
{
	p.x = mapdouble(p.x);
	p.y = mapdouble(p.y);
	return p;
}

arc maparc(arc a)
{
	a.center = mappoint(a.center);
	if(!a.end.x)
		return a;
	//push back some new parms for the radius/startangle/endangle
	double dex = *a.end.x - *a.center.x;
	double dey = *a.end.y - *a.center.y;
	double dsx = *a.start.x - *a.center.x;
	double dsy = *a.start.y - *a.center.y;

	double rad = (sqrt(dex*dex+dey*dey) + sqrt(dsx*dsx+dsy*dsy))/2;

	if(parmmap[a.start.x])
	{
		a.rad = parmmap[a.start.x];
		a.startAngle = parmmap[a.start.y];
		a.endAngle = parmmap[a.end.x];
		return a;
	}

	int idx = parmdata.size();
	parmdata.push_back(rad);
	parmdata.push_back(atan2(dsx,dsy));
	parmdata.push_back(atan2(dex,dey));

	//push the pointers to the doubles on the parmstack
	oldparms.insert(&parmdata[idx]);
	oldparms.insert(&parmdata[idx+1]);
	oldparms.insert(&parmdata[idx+2]);

	a.rad = mapdouble(&parmdata[idx]);
	a.startAngle = mapdouble(&parmdata[idx+1]);
	a.endAngle = mapdouble(&parmdata[idx+2]);

	parmmap[a.start.x] = a.rad;
	parmmap[a.start.y] = a.startAngle;
	parmmap[a.end.x] = a.endAngle;
	return a;
}

void unmaparc(arc a)
{
#ifdef NEWARC
	if(!a.end.x)
		return;
	*a.end.x = sin(*a.endAngle)* *a.rad + *a.center.x;
	*a.end.y = cos(*a.endAngle)* *a.rad + *a.center.y;
	*a.start.x = sin(*a.startAngle)* *a.rad + *a.center.x;
	*a.start.y = cos(*a.startAngle)* *a.rad + *a.center.y;
#endif
}

circle mapcircle(circle c)
{
	c.center = mappoint(c.center);
	c.rad = mapdouble(c.rad);
	return c;
}

ellipse mapellipse(ellipse e)
{
	e.center = mappoint(e.center);
	e.radone = mapdouble(e.radone);
	e.radtwo = mapdouble(e.radtwo);
	e.rot = mapdouble(e.rot);
	return e;
}

line mapline(line l)
{
	l.p1 = mappoint(l.p1);
	l.p2 = mappoint(l.p2);
	return l;
}

int solvewpoints(double  **parms,int nparms, constraint * cons, int consLength, std::set<double*>fixed, int isFine)
{
	parmdata.clear();
	newparms.clear();
	parmmap.clear();
	rparmmap.clear();
	newcons.clear();
	usedparms.clear();
	hasusedparms.clear();
	oldparms.clear();
	arccons.clear();

	//This keeps parmdata's pointers from moving because of reallocation
	parmdata.reserve(nparms);

	//create a set of the old parameters
	for(int i=0; i < nparms; i++)
		oldparms.insert(parms[i]);


    for(int i=0; i < consLength; i++)
	{
		if(cons[i].type == pointOnPoint)
		{
			//check to see if there is an existing pointonpoint constraint related to this one
			//TODO: this will blow up if there are more than 3 coincident points and the constraints 
			//are not ordered so that this always returns true
			if(parmmap[cons[i].point1.x] || parmmap[cons[i].point2.x])
			{
				if(parmmap[cons[i].point1.x] && parmmap[cons[i].point2.x])
				{
					//BOOM, need to merge the maps, maybe somehow delete the pointers from parmdata, probably 
					//another pass is required
				}
				//figure out which point has been used before
				if(parmmap[cons[i].point1.x])
				{
					//Associate the new doubles with the same pointers
					parmmap[cons[i].point2.x] = parmmap[cons[i].point1.x];
					parmmap[cons[i].point2.y] = parmmap[cons[i].point1.y];
					rparmmap[parmmap[cons[i].point2.x]].push_back(cons[i].point2.x);
					rparmmap[parmmap[cons[i].point2.y]].push_back(cons[i].point2.y);

					//Try and average out the value a bit
					*parmmap[cons[i].point2.x] = *parmmap[cons[i].point2.x] * 2.0 / 3.0 + *cons[i].point2.x / 3.0;
					*parmmap[cons[i].point2.y] = *parmmap[cons[i].point2.y] * 2.0 / 3.0 + *cons[i].point2.y / 3.0;

				}
				else
				{
					parmmap[cons[i].point1.x] = parmmap[cons[i].point2.x];
					parmmap[cons[i].point1.y] = parmmap[cons[i].point2.y];
					rparmmap[parmmap[cons[i].point2.x]].push_back(cons[i].point1.x);
					rparmmap[parmmap[cons[i].point2.y]].push_back(cons[i].point1.y);

					*parmmap[cons[i].point2.x] = *parmmap[cons[i].point2.x] * 2.0 / 3.0 + *cons[i].point1.x / 3.0;
					*parmmap[cons[i].point2.y] = *parmmap[cons[i].point2.y] * 2.0 / 3.0 + *cons[i].point1.y / 3.0;
				}
			}
			else
			{
				int idx = parmdata.size();
				//create some new doubles to point at. initialized to the midpoint
				parmdata.push_back(*cons[i].point1.x/2 + *cons[i].point2.x/2);
				parmdata.push_back(*cons[i].point1.y/2 + *cons[i].point2.y/2);
				//push some pointers to the new doubles
				newparms.push_back(&parmdata[idx]);
				newparms.push_back(&parmdata[idx+1]);
				//create associatation between old pointer and the new doubles
				parmmap[cons[i].point1.x] = &parmdata[idx];
				parmmap[cons[i].point1.y] = &parmdata[idx+1]; 
				parmmap[cons[i].point2.x] = &parmdata[idx];
				parmmap[cons[i].point2.y] = &parmdata[idx+1];
				//create association between new doubles and the old pointer
				rparmmap[&parmdata[idx]].push_back(cons[i].point1.x);
				rparmmap[&parmdata[idx+1]].push_back(cons[i].point1.y);
				rparmmap[&parmdata[idx]].push_back(cons[i].point2.x);
				rparmmap[&parmdata[idx+1]].push_back(cons[i].point2.y);
			}
		}
		else
#ifdef NEWARC
			//put the constraint in a new list, pointonpoint free
			if(cons[i].type == arcRules)
				arccons.push_back(cons[i]);
			else
#endif
				newcons.push_back(cons[i]);
	}

	for(std::vector<constraint>::size_type i=0; i < newcons.size(); i++)
	{
		//map all pointers
		newcons[i].arc1 = maparc(newcons[i].arc1);
		newcons[i].arc2 = maparc(newcons[i].arc2);
		newcons[i].ellipse1 = mapellipse(newcons[i].ellipse1);
		newcons[i].ellipse2 = mapellipse(newcons[i].ellipse2);
		newcons[i].circle1 = mapcircle(newcons[i].circle1);
		newcons[i].circle2 = mapcircle(newcons[i].circle2);
		newcons[i].line1 = mapline(newcons[i].line1);
		newcons[i].line2 = mapline(newcons[i].line2);
		newcons[i].parameter = mapdouble(newcons[i].parameter);
		newcons[i].point1 = mappoint(newcons[i].point1);
		newcons[i].point2 = mappoint(newcons[i].point2);
		newcons[i].SymLine = mapline(newcons[i].SymLine);
	}

	for(std::vector<constraint>::size_type i=0; i < arccons.size(); i++)
	{
		//map all arcrules
		arccons[i].arc1 = maparc(arccons[i].arc1);
		arccons[i].arc2 = maparc(arccons[i].arc2);
	}


	std::set<double *> toremove;
	//Find any parms that are coincident with a fixed parm
	std::set<double*>::iterator it;
	for(it = fixed.begin(); it != fixed.end(); ++it)
	{
		double *v = *it;
		toremove.insert(v);

		if(parmmap[v])
		{
			toremove.insert(parmmap[v]);

			std::list<double*> plist = rparmmap[parmmap[v]];
			std::list<double*>::iterator it2;
			for(it2 = plist.begin(); it2 != plist.end(); it2++)
			{
				if(*it2 != v)
					toremove.insert(*it2);
			}
		}
	}

	std::vector<double*> evennewerparms;
	for(size_t i=0; i < usedparms.size(); i++)
	{
		if(toremove.find(usedparms[i]) == toremove.end())
			evennewerparms.push_back(usedparms[i]);
	}
	    
	int ret = 0;
	if(newcons.size())
		ret = solve.solve(&evennewerparms[0],evennewerparms.size(),&newcons[0],newcons.size(),isFine);


	//loop through all remapped pointers
	for(std::vector<double*>::size_type i=0; i < newparms.size(); i++)
	{
		if(rparmmap[newparms[i]].size() > 0)
		{
			std::list<double*>::iterator it;
			for(it = rparmmap[newparms[i]].begin(); it != rparmmap[newparms[i]].end(); ++it)
			{
				//update
				**it = *newparms[i];
			}
		}
	}

	//fix up the arcs
	for(std::vector<constraint>::size_type i=0; i < arccons.size(); i++)
	{
		unmaparc(arccons[i].arc1);
		unmaparc(arccons[i].arc2);
	}

	return ret;
}
